/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "oneflow/core/functional/function_library.h"
#include "oneflow/core/framework/id_util.h"
#include "oneflow/core/framework/tensor.h"
#include "oneflow/core/framework/tensor_tuple.h"
#include "oneflow/core/framework/op_builder.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/nd_sbp.h"
#include "oneflow/core/functional/functional.h"
#include "oneflow/core/operator/operator.h"
#include "oneflow/core/autograd/autograd_mode.h"
#include "oneflow/core/autograd/autograd_engine.h"
#include "oneflow/core/framework/op_expr_helper.h"
#include "oneflow/core/framework/tensor_rpc_util.h"
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/job/global_for.h"
#include "oneflow/core/job/resource_desc.h"
#include "oneflow/core/job/rank_group_scope.h"
#include "oneflow/core/job/lazy_mode.h"
#include "oneflow/core/framework/transport_token.h"
#include "oneflow/core/framework/transport_util.h"
#include "oneflow/core/framework/placement_sbp_util.h"
#include "oneflow/core/object_msg/flat_msg.h"
#include "oneflow/core/common/flat_shape.h"
#include "oneflow/core/common/container_util.h"
#include "oneflow/core/common/balanced_splitter.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/common/optional.h"
#include "oneflow/core/ccl/ccl.h"

namespace oneflow {
namespace one {
namespace functional {

namespace impl {

namespace {

// clang-format off
FLAT_MSG_BEGIN(FlatShapeAndDataType);
  // Methods
  OF_PUBLIC static Maybe<FlatShapeAndDataType> New() {
    const auto& flat_shape_dtype = std::make_shared<FlatShapeAndDataType>();
    flat_shape_dtype->clear();
    return flat_shape_dtype;
  }
  OF_PUBLIC static Maybe<FlatShapeAndDataType> New(const Shape& shape, DataType dtype) {
    const auto& flat_shape_dtype = JUST(New());
    JUST(flat_shape_dtype->mutable_shape()->Init(shape));
    flat_shape_dtype->set_dtype(dtype);
    return flat_shape_dtype;
  }
  OF_PUBLIC Maybe<void> Check(const Shape& shape, DataType dtype) const {
    JUST(this->shape().Check(shape));
    CHECK_EQ_OR_RETURN(this->dtype(), dtype);
    return Maybe<void>::Ok();
  }
  OF_PUBLIC Maybe<void> ToShape(Shape* shape) const { return this->shape().ToShape(shape); }
  OF_PUBLIC Maybe<Shape> ToShape() const { return shape().ToShape(); }
  OF_PUBLIC int64_t At(int i) const { return shape().At(i); }
  OF_PUBLIC int64_t NumAxes() const { return shape().NumAxes(); }

  // Fields
  FLAT_MSG_DEFINE_OPTIONAL(FlatShape, shape);
  FLAT_MSG_DEFINE_OPTIONAL(DataType, dtype);

FLAT_MSG_END(FlatShapeAndDataType);
// clang-format on

Maybe<HashMap<int64_t, std::shared_ptr<FlatShapeAndDataType>>> BroadcastGatherShapeAndDataType(
    const Shape& shape, DataType dtype, Symbol<ParallelDesc> parallel_desc) {
  const auto& transport_token =
      JUST(TransportToken::NewTransportToken(kTransportTokenTypeSyncLocalShapeDtype));
  const auto& send_buffer = JUST(FlatShapeAndDataType::New(shape, dtype));
  const auto& map = std::make_shared<HashMap<int64_t, std::shared_ptr<FlatShapeAndDataType>>>();
  map->emplace(GlobalProcessCtx::Rank(), send_buffer);
  NaiveAsyncTransportCtx ctx(
      transport_token,
      [send_buffer](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
        *buffer = send_buffer.get();
        *size = sizeof(FlatShapeAndDataType);
        *Cb = [send_buffer] {};
        return Maybe<void>::Ok();
      },
      [map](int64_t rank, void** buffer, std::size_t* size,
            std::function<void()>* Cb) -> Maybe<void> {
        const auto& recv_buffer = JUST(FlatShapeAndDataType::New());
        recv_buffer->clear();
        *buffer = recv_buffer.get();
        *size = sizeof(FlatShapeAndDataType);
        *Cb = [recv_buffer] {};
        CHECK_OR_RETURN(map->emplace(rank, recv_buffer).second);
        return Maybe<void>::Ok();
      });
  const auto& src_ranks = JUST(RankGroup::New(parallel_desc));
  const auto& dst_ranks = JUST(RankGroupScope::CurrentRankGroup());
  JUST(TransportUtil::BroadcastToOtherRanks(src_ranks, dst_ranks, transport_token, &ctx));
  JUST(TransportUtil::CollectFromOtherRanks(src_ranks, dst_ranks, transport_token, &ctx));
  JUST(TransportUtil::WaitUntilDoneOrTimeout(ctx, TransportUtil::TimeoutSeconds()));
  return map;
}

Maybe<int64_t> FindRoot(Symbol<ParallelDesc> broadcast_parallel_desc,
                        Symbol<ParallelDesc> src_parallel_desc) {
  for (int64_t process_id : broadcast_parallel_desc->sorted_machine_ids()) {
    if (src_parallel_desc->ContainingMachineId(process_id)) { return process_id; }
  }
  UNIMPLEMENTED_THEN_RETURN();
}

auto* CachedFindRoot = DECORATE(&FindRoot, ThreadLocal);

Maybe<FlatShapeAndDataType> BroadcastShapeAndDtype(const Shape& shape, DataType dtype,
                                                   Symbol<ParallelDesc> parallel_desc) {
  const auto& rank_group = JUST(RankGroupScope::CurrentRankGroup());
  const auto& rank_group_parallel_desc =
      JUST(RankGroup::GetDefaultParallelDesc(parallel_desc->device_type(), rank_group));
  const auto& process_id2broadcast_group =
      JUST(GetBroadcastGroupWithoutAcrossNode(parallel_desc, rank_group_parallel_desc));
  const auto& broadcast_parallel_desc =
      JUST(MapAt(*process_id2broadcast_group, GlobalProcessCtx::Rank()));

  const auto& in_flat_shape_dtype = JUST(FlatShapeAndDataType::New(shape, dtype));
  const auto& out_flat_shape_dtype = JUST(FlatShapeAndDataType::New());
  int64_t root = JUST(CachedFindRoot(broadcast_parallel_desc, parallel_desc));
  const auto& transport_token =
      JUST(TransportToken::NewTransportToken(kTransportTokenTypeSyncLocalShapeDtype));
  JUST(ccl::CpuBroadcast(in_flat_shape_dtype.get(), out_flat_shape_dtype.get(),
                         sizeof(FlatShapeAndDataType), root, broadcast_parallel_desc,
                         transport_token));
  return out_flat_shape_dtype;
}

Maybe<void> GetConcatenatedShapeAndCheckDtype(
    Shape* logical_shape, DataType* dtype,
    const HashMap<int64_t, std::shared_ptr<FlatShapeAndDataType>>& rank2flat_shape_dtype,
    Symbol<ParallelDesc> parallel_desc, int64_t concat_axis) {
  const auto& GetRankPhyShapeByParallelId =
      [&](int64_t parallel_id) -> Maybe<FlatShapeAndDataType> {
    int64_t machine_id = JUST(parallel_desc->MachineId4ParallelId(parallel_id));
    return JUST(MapAt(rank2flat_shape_dtype, machine_id));
  };
  const auto& first_flat_shape_dtype = JUST(GetRankPhyShapeByParallelId(0));
  CHECK_GE_OR_RETURN(concat_axis, 0);
  CHECK_LT_OR_RETURN(concat_axis, first_flat_shape_dtype->NumAxes());
  int64_t logical_concat_dim = first_flat_shape_dtype->At(concat_axis);
  for (int parallel_id = 1; parallel_id < parallel_desc->parallel_num(); ++parallel_id) {
    const auto& rank_flat_shape_dtype = JUST(GetRankPhyShapeByParallelId(parallel_id));
    CHECK_EQ_OR_RETURN(rank_flat_shape_dtype->NumAxes(), first_flat_shape_dtype->NumAxes());
    logical_concat_dim += rank_flat_shape_dtype->At(concat_axis);
  }
  BalancedSplitter bs(logical_concat_dim, parallel_desc->parallel_num());
  CHECK_EQ_OR_RETURN(first_flat_shape_dtype->At(concat_axis), bs.At(0).size());
  JUST(first_flat_shape_dtype->ToShape(logical_shape));
  logical_shape->Set(concat_axis, logical_concat_dim);
  *dtype = first_flat_shape_dtype->dtype();
  for (int parallel_id = 1; parallel_id < parallel_desc->parallel_num(); ++parallel_id) {
    const auto& rank_flat_shape_dtype = JUST(GetRankPhyShapeByParallelId(parallel_id));
    for (int i = 0; i < logical_shape->NumAxes(); ++i) {
      if (i == concat_axis) {
        CHECK_EQ_OR_RETURN(rank_flat_shape_dtype->At(i), bs.At(parallel_id).size());
      } else {
        CHECK_EQ_OR_RETURN(rank_flat_shape_dtype->At(i), logical_shape->At(i));
      }
    }
    CHECK_EQ_OR_RETURN(*dtype, rank_flat_shape_dtype->dtype());
  }
  return Maybe<void>::Ok();
}

Maybe<void> GetLogicalShapeAndDataType(Shape* logical_shape, DataType* /* in and out */ dtype,
                                       std::shared_ptr<const Shape> physical_shape,
                                       Symbol<ParallelDesc> parallel_desc,
                                       Symbol<cfg::NdSbp> nd_sbp) {
  if (nd_sbp->sbp_parallel_size() == 1 && nd_sbp->sbp_parallel(0).has_split_parallel()) {
    const auto& rank2flat_shape_dtype =
        JUST(BroadcastGatherShapeAndDataType(*physical_shape, *dtype, parallel_desc));
    int64_t concat_axis = nd_sbp->sbp_parallel(0).split_parallel().axis();
    JUST(GetConcatenatedShapeAndCheckDtype(logical_shape, dtype, *rank2flat_shape_dtype,
                                           parallel_desc, concat_axis));
  } else {
    if (JUST(RankGroup::New(parallel_desc)) != JUST(RankGroupScope::CurrentRankGroup())) {
      const auto& flat_shape_dtype =
          JUST(BroadcastShapeAndDtype(*physical_shape, *dtype, parallel_desc));
      physical_shape = JUST(flat_shape_dtype->ToShape());
      *dtype = flat_shape_dtype->dtype();
    }
    *logical_shape = *JUST(GetLogicalShape(*physical_shape, *nd_sbp, *parallel_desc));
  }
  return Maybe<void>::Ok();
}

namespace {

Maybe<one::OpExpr> RawGetConsistentToConsistentOpExpr(
    const std::vector<Symbol<cfg::SbpParallel>>& grad_sbp_parallels) {
  Optional<Symbol<cfg::NdSbp>> grad_nd_sbp;
  if (!grad_sbp_parallels.empty()) { grad_nd_sbp = JUST(GetNdSbp(grad_sbp_parallels)); }
  std::shared_ptr<one::OpExpr> op_expr = JUST(one::ConsistentToConsistentOpExpr::New(grad_nd_sbp));
  return op_expr;
}

}  // namespace

static constexpr auto* GetConsistentToConsistentOpExpr =
    DECORATE(&RawGetConsistentToConsistentOpExpr, ThreadLocalCopiable);

Maybe<Tensor> ConsistentToConsistent(
    const std::shared_ptr<Tensor>& x, Symbol<ParallelDesc> parallel_desc,
    const std::vector<Symbol<cfg::SbpParallel>>& sbp_parallels,
    const std::vector<Symbol<cfg::SbpParallel>>& grad_sbp_parallels) {
  const auto& consistent_tensor = JUST(x->AsConsistentTensor());
  CHECK_NOTNULL_OR_RETURN(consistent_tensor) << "consistent tensors supported only";
  const auto& op = JUST(GetConsistentToConsistentOpExpr(grad_sbp_parallels));
  const auto& nd_sbp = JUST(GetNdSbp(sbp_parallels));
  const auto& tensor = JUST(OpInterpUtil::Dispatch<one::Tensor>(
      *op, {consistent_tensor}, OpExprInterpContext(AttrMap{}, parallel_desc, nd_sbp)));
  if (!LazyMode::is_enabled() && tensor != x && !IsConsistentTensorMetaCheckDisabled()) {
    const auto& input_consistent_id = JUST(x->transport_token());
    const auto& output_consistend_id = JUST(tensor->transport_token());
    CHECK_NE_OR_RETURN(input_consistent_id, output_consistend_id);
  }
  return tensor;
}

Maybe<Tensor> LocalToConsistent(const std::shared_ptr<Tensor>& x,
                                Symbol<ParallelDesc> parallel_desc,
                                const std::vector<Symbol<cfg::SbpParallel>>& sbp_parallels,
                                const std::shared_ptr<OpExpr>& op) {
  CHECK_OR_RETURN(!x->is_lazy())
      << "local_tensor.to_consistent() is not supported within nn.Graph for now";
  CHECK_OR_RETURN(x->is_local()) << Error::UnimplementedError() << "local tensors supported only";
  std::shared_ptr<one::Tensor> input = x;
  // copy to right device first if input's device type is wrong
  if (JUST(JUST(input->device())->of_type()) != parallel_desc->device_tag()) {
    LOG(INFO) << "The device_type of the input tensor is different from placement, now copy it to "
              << Device::Type4DeviceTag(parallel_desc->device_tag());
    input = JUST(functional::Copy(x, Device::Type4DeviceTag(parallel_desc->device_tag()),
                                  GlobalProcessCtx::LocalRank()));
  }
  // copy to default device of the current rank if input's device type is right but not on default
  // device
  if (JUST(input->device())->device_id() != GlobalProcessCtx::LocalRank()) {
    LOG(INFO) << "The tensor isn't on default device of the current rank., now copy it to "
              << parallel_desc->device_tag() << ": " << GlobalProcessCtx::LocalRank();
    input = JUST(functional::Copy(x, Device::Type4DeviceTag(parallel_desc->device_tag()),
                                  GlobalProcessCtx::LocalRank()));
  }
  const auto& device = JUST(input->device());
  CHECK_EQ_OR_RETURN(JUST(device->of_type()), parallel_desc->device_tag())
      << Error::UnimplementedError() << "tensor' device type must be same with placement.";
  CHECK_EQ_OR_RETURN(device->device_id(), GlobalProcessCtx::LocalRank())
      << Error::UnimplementedError() << "tensor must be on default device of the current rank.";
  Symbol<cfg::NdSbp> nd_sbp = JUST(GetNdSbp(sbp_parallels));
  const auto& shape = std::make_shared<Shape>();
  DataType dtype = x->dtype()->data_type();
  JUST(GetLogicalShapeAndDataType(shape.get(), &dtype, x->shape(), parallel_desc, nd_sbp));
  MutableAttrMap attrs;
  JUST(attrs.SetAttr<Shape>("shape", *shape));
  JUST(attrs.SetAttr<DataType>("dtype", dtype));
  const auto& output = JUST(OpInterpUtil::Dispatch<one::Tensor>(
      *op, {input}, OpExprInterpContext(attrs, parallel_desc, nd_sbp)));
  return output;
}

}  //  namespace

class LocalToConsistentFunctor {
 public:
  LocalToConsistentFunctor() {
    op_ =
        CHECK_JUST(one::CastToConsistentOpExpr::New(*CHECK_JUST(UniqueStr("cast_to_consistent"))));
  }

  Maybe<Tensor> operator()(const std::shared_ptr<one::Tensor>& x,
                           Symbol<ParallelDesc> parallel_desc,
                           const std::vector<Symbol<cfg::SbpParallel>>& sbp_parallels,
                           const Shape& shape, const Symbol<DType>& dtype) const {
    CHECK_OR_RETURN(x->is_local());
    Symbol<cfg::NdSbp> nd_sbp = JUST(GetNdSbp(sbp_parallels));
    MutableAttrMap attrs;
    JUST(attrs.SetAttr<Shape>("shape", shape));
    JUST(attrs.SetAttr<DataType>("dtype", dtype->data_type()));
    const auto& tensor = JUST(OpInterpUtil::Dispatch<one::Tensor>(
        *op_, {x}, OpExprInterpContext(attrs, parallel_desc, nd_sbp)));
    return tensor;
  }

 private:
  std::shared_ptr<OpExpr> op_;
};

class ToConsistentFunctor {
 public:
  ToConsistentFunctor() {
    local_to_consistent_op_ =
        CHECK_JUST(one::CastToConsistentOpExpr::New(*CHECK_JUST(UniqueStr("cast_to_consistent"))));
  }

  Maybe<Tensor> operator()(const std::shared_ptr<one::Tensor>& x,
                           Symbol<ParallelDesc> parallel_desc,
                           const std::vector<Symbol<cfg::SbpParallel>>& sbp_parallels,
                           const std::vector<Symbol<cfg::SbpParallel>>& grad_sbp_parallels) const {
    std::shared_ptr<Tensor> tensor;
    if (x->is_consistent()) {
      tensor = JUST(ConsistentToConsistent(x, parallel_desc, sbp_parallels, grad_sbp_parallels));
    } else {
      tensor = JUST(LocalToConsistent(x, parallel_desc, sbp_parallels, local_to_consistent_op_));
    }
    return tensor;
  }

 private:
  std::shared_ptr<OpExpr> local_to_consistent_op_;
};

class ConsistentToLocalFunctor {
 public:
  ConsistentToLocalFunctor() {
    op_ = CHECK_JUST(
        one::CastFromConsistentOpExpr::New(*CHECK_JUST(UniqueStr("consistent_to_local"))));
  }

  Maybe<Tensor> operator()(const std::shared_ptr<one::Tensor>& x) const {
    CHECK_OR_RETURN(!x->is_lazy())
        << "consistent_tensor.to_local() is not supported within nn.Graph for now";
    CHECK_OR_RETURN(x->is_consistent()) << "consistent tensors supported only";
    return JUST(OpInterpUtil::Dispatch<one::Tensor>(*op_, {x}));
  }

 private:
  std::shared_ptr<OpExpr> op_;
};

}  // namespace impl

ONEFLOW_FUNCTION_LIBRARY(m) {
  m.add_functor<impl::LocalToConsistentFunctor>("LocalToConsistent");
  m.add_functor<impl::ToConsistentFunctor>("ToConsistent");
  m.add_functor<impl::ConsistentToLocalFunctor>("ConsistentToLocal");
};

}  // namespace functional
}  // namespace one
}  // namespace oneflow
