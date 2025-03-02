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
#include "oneflow/core/common/container_util.h"
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/device/nccl_util.h"
#include "oneflow/core/job/eager_nccl_comm_manager.h"
#include "oneflow/core/job/parallel_desc.h"
#include "oneflow/core/kernel/new_kernel_util.h"

namespace oneflow {

namespace {

class EagerNcclOpKernelState final : public user_op::OpKernelState {
 public:
  EagerNcclOpKernelState(user_op::KernelInitContext* ctx) { Init(ctx); }
  ~EagerNcclOpKernelState() override = default;

  Symbol<ParallelDesc> parallel_desc() const { return parallel_desc_; }
  ncclComm_t comm() const { return comm_; }

 private:
  void Init(user_op::KernelInitContext* ctx) {
    const std::string& parallel_conf_txt = ctx->Attr<std::string>("parallel_conf");
    ParallelConf parallel_conf;
    std::set<std::pair<int64_t, int64_t>> device_set;
    CHECK(TxtString2PbMessage(parallel_conf_txt, &parallel_conf));
    parallel_desc_ = SymbolOf(ParallelDesc(parallel_conf));
    FOR_RANGE(int64_t, parallel_id, 0, parallel_desc_->parallel_num()) {
      int64_t machine_id = CHECK_JUST(parallel_desc_->MachineId4ParallelId(parallel_id));
      int64_t device_id = CHECK_JUST(parallel_desc_->DeviceId4ParallelId(parallel_id));
      device_set.emplace(std::make_pair(machine_id, device_id));
    }
    comm_ = CHECK_NOTNULL(Global<EagerNcclCommMgr>::Get())->GetCommForDevice(device_set);
  }

  Symbol<ParallelDesc> parallel_desc_;
  ncclComm_t comm_;
};

size_t InferEagerNcclS2SKernelTmpBufferSize(user_op::InferContext* ctx) {
  const user_op::TensorDesc& in_tensor = ctx->InputTensorDesc("in", 0);
  size_t tensor_byte_size =
      GetCudaAlignedSize(in_tensor.shape().elem_cnt() * GetSizeOfDataType(in_tensor.data_type()));
  // NOTE(hanbinbin): Set tmp_buffer_size to twice tensor_byte_size because the
  // SbpParallel4ArgNameAndIndex function of LocalUserOpInferContext is unimplemented
  return tensor_byte_size * 2;
}

}  // namespace

class EagerNcclAllReduceKernel final : public user_op::OpKernel {
 public:
  EagerNcclAllReduceKernel() = default;
  ~EagerNcclAllReduceKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(in->shape(), out->shape());
    CHECK_EQ(in->data_type(), out->data_type());
    OF_NCCL_CHECK(ncclAllReduce(in->dptr(), out->mut_dptr(), in->shape().elem_cnt(),
                                GetNcclDataType(in->data_type()), ncclSum, kernel_state->comm(),
                                ctx->device_ctx()->cuda_stream()));
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

REGISTER_USER_KERNEL("eager_nccl_all_reduce")
    .SetCreateFn<EagerNcclAllReduceKernel>()
    .SetIsMatchedHob(user_op::HobDeviceTag() == "gpu");

class EagerNcclBroadcastKernel final : public user_op::OpKernel {
 public:
  EagerNcclBroadcastKernel() = default;
  ~EagerNcclBroadcastKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    int64_t root = ctx->Attr<int64_t>("root");
    int64_t dev_id = GlobalProcessCtx::LocalRank(root);
    int64_t nccl_root =
        CHECK_JUST(kernel_state->parallel_desc()->ParallelId4MachineDeviceId(root, dev_id));
    const void* in_ptr = nullptr;
    if (GlobalProcessCtx::Rank() == root) {
      CHECK_EQ(in->shape(), out->shape());
      CHECK_EQ(in->data_type(), out->data_type());
      in_ptr = in->dptr();
    }
    OF_NCCL_CHECK(ncclBroadcast(in_ptr, out->mut_dptr(), out->shape().elem_cnt(),
                                GetNcclDataType(out->data_type()), nccl_root, kernel_state->comm(),
                                ctx->device_ctx()->cuda_stream()));
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

REGISTER_USER_KERNEL("eager_nccl_broadcast")
    .SetCreateFn<EagerNcclBroadcastKernel>()
    .SetIsMatchedHob(user_op::HobDeviceTag() == "gpu");

class EagerNcclReduceKernel final : public user_op::OpKernel {
 public:
  EagerNcclReduceKernel() = default;
  ~EagerNcclReduceKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(in->shape(), out->shape());
    CHECK_EQ(in->data_type(), out->data_type());
    int64_t root = ctx->Attr<int64_t>("root");
    OF_NCCL_CHECK(ncclReduce(in->dptr(), out->mut_dptr(), in->shape().elem_cnt(),
                             GetNcclDataType(in->data_type()), ncclSum, root, kernel_state->comm(),
                             ctx->device_ctx()->cuda_stream()));
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

REGISTER_USER_KERNEL("eager_nccl_reduce")
    .SetCreateFn<EagerNcclReduceKernel>()
    .SetIsMatchedHob(user_op::HobDeviceTag() == "gpu");

class EagerNcclReduceScatterKernel final : public user_op::OpKernel {
 public:
  EagerNcclReduceScatterKernel() = default;
  ~EagerNcclReduceScatterKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(in->data_type(), out->data_type());
    const auto& op_type = ctx->Attr<std::string>("op_type");
    OF_NCCL_CHECK(ncclReduceScatter(in->dptr(), out->mut_dptr(), out->shape().elem_cnt(),
                                    GetNcclDataType(in->data_type()),
                                    CHECK_JUST(MapAt(op_type2ncclRedOp_t, op_type)),
                                    kernel_state->comm(), ctx->device_ctx()->cuda_stream()));
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

  static HashMap<std::string, ncclRedOp_t> op_type2ncclRedOp_t;
};

HashMap<std::string, ncclRedOp_t> EagerNcclReduceScatterKernel::op_type2ncclRedOp_t = {
    {"sum", ncclSum}, {"max", ncclMax}};

REGISTER_USER_KERNEL("eager_nccl_reduce_scatter")
    .SetCreateFn<EagerNcclReduceScatterKernel>()
    .SetIsMatchedHob(user_op::HobDeviceTag() == "gpu");

class EagerNcclAllGatherKernel final : public user_op::OpKernel {
 public:
  EagerNcclAllGatherKernel() = default;
  ~EagerNcclAllGatherKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(in->data_type(), out->data_type());
    OF_NCCL_CHECK(ncclAllGather(in->dptr(), out->mut_dptr(), in->shape().elem_cnt(),
                                GetNcclDataType(in->data_type()), kernel_state->comm(),
                                ctx->device_ctx()->cuda_stream()));
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

REGISTER_USER_KERNEL("eager_nccl_all_gather")
    .SetCreateFn<EagerNcclAllGatherKernel>()
    .SetIsMatchedHob(user_op::HobDeviceTag() == "gpu");

template<typename T>
class EagerNcclS2SKernel final : public user_op::OpKernel {
 public:
  EagerNcclS2SKernel() = default;
  ~EagerNcclS2SKernel() override = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    return std::make_shared<EagerNcclOpKernelState>(ctx);
  }

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    auto* kernel_state = dynamic_cast<EagerNcclOpKernelState*>(state);
    CHECK(kernel_state != nullptr);
    // NOTE(hanbinbin): Compute logic copy from _nccl_logical_s2s
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    user_op::Tensor* tmp_buffer = ctx->Tensor4ArgNameAndIndex("tmp_buffer", 0);
    int64_t tmp_size = 0;
    const int64_t dtype_size = GetSizeOfDataType(in->data_type());
    int64_t data_size = GetCudaAlignedSize(in->shape().elem_cnt() * dtype_size);
    // NOTE(chengcheng): in (transpose)-> pack_to_ptr (all2all)-> unpack_from_ptr (transpose)-> out
    const char* pack_to_ptr = in->dptr<char>();
    char* unpack_from_ptr = out->mut_dptr<char>();
    if (tmp_buffer) { tmp_size = tmp_buffer->shape().elem_cnt(); }
    CHECK(tmp_size == 0 || tmp_size == data_size || tmp_size == data_size * 2);

    CHECK_EQ(in->data_type(), out->data_type());
    const int64_t num_ranks = kernel_state->parallel_desc()->parallel_num();
    CHECK_EQ(in->shape().elem_cnt(), out->shape().elem_cnt());
    const int64_t elem_cnt = in->shape().elem_cnt();
    const int64_t in_split_axis = ctx->Attr<int64_t>("in_split_axis");
    const int64_t out_split_axis = ctx->Attr<int64_t>("out_split_axis");

    DimVector logical_shape_dim_vec;
    in->shape().ToDimVector(&logical_shape_dim_vec);
    logical_shape_dim_vec[in_split_axis] = logical_shape_dim_vec.at(in_split_axis) * num_ranks;

    if (out_split_axis != 0) {
      // NOTE(chengcheng): Do pack. Need transpose in -> pack_to
      // pack use temp buffer offset: [0, data_size]
      pack_to_ptr = tmp_buffer->dptr<char>();
      DimVector transpose_in_dim_vec = logical_shape_dim_vec;
      CHECK_EQ(transpose_in_dim_vec.at(in_split_axis) % num_ranks, 0);
      transpose_in_dim_vec[in_split_axis] = transpose_in_dim_vec.at(in_split_axis) / num_ranks;
      CHECK_EQ(transpose_in_dim_vec.at(out_split_axis) % num_ranks, 0);
      transpose_in_dim_vec[out_split_axis] = transpose_in_dim_vec.at(out_split_axis) / num_ranks;
      transpose_in_dim_vec.insert(transpose_in_dim_vec.begin() + out_split_axis, num_ranks);
      const Shape transpose_in_shape(transpose_in_dim_vec);
      DimVector pack_to_dim_vec;
      std::vector<int32_t> perm;
      perm.push_back(out_split_axis);
      pack_to_dim_vec.push_back(transpose_in_shape.At(out_split_axis));
      FOR_RANGE(int64_t, i, 0, transpose_in_shape.NumAxes()) {
        if (i != out_split_axis) {
          perm.push_back(i);
          pack_to_dim_vec.push_back(transpose_in_shape.At(i));
        }
      }
      CHECK_EQ(elem_cnt, transpose_in_shape.elem_cnt());
      const Shape pack_to_shape(pack_to_dim_vec);
      CHECK_EQ(elem_cnt, pack_to_shape.elem_cnt());
      NewKernelUtil<DeviceType::kGPU>::Transpose(
          ctx->device_ctx(), transpose_in_shape.NumAxes(), transpose_in_shape, pack_to_shape, perm,
          elem_cnt, in->dptr<T>(), reinterpret_cast<T*>(tmp_buffer->mut_dptr<char>()));
    }

    if (in_split_axis != 0) {
      // NOTE(chengcheng): Do unpack. Need transpose unpack_from -> out
      // unpack use temp buffer offset: [tmp_size - data_size, tmp_size]
      unpack_from_ptr = tmp_buffer->mut_dptr<char>() + (tmp_size - data_size);
    }

    {
      // NOTE: Do S2S
      OF_NCCL_CHECK(ncclGroupStart());
      const int64_t elem_per_chunk = elem_cnt / num_ranks;
      const int64_t chunk_size = elem_per_chunk * dtype_size;
      for (int64_t j = 0; j < num_ranks; ++j) {
        OF_NCCL_CHECK(ncclSend(reinterpret_cast<const void*>(
                                   reinterpret_cast<const char*>(pack_to_ptr) + j * chunk_size),
                               elem_per_chunk, GetNcclDataType(in->data_type()), j,
                               kernel_state->comm(), ctx->device_ctx()->cuda_stream()));
        OF_NCCL_CHECK(ncclRecv(
            reinterpret_cast<void*>(reinterpret_cast<char*>(unpack_from_ptr) + j * chunk_size),
            elem_per_chunk, GetNcclDataType(in->data_type()), j, kernel_state->comm(),
            ctx->device_ctx()->cuda_stream()));
      }
      OF_NCCL_CHECK(ncclGroupEnd());
    }

    if (in_split_axis != 0) {
      // Do unpack.
      CHECK(unpack_from_ptr != out->mut_dptr<char>());
      DimVector unpack_from_dim_vec = logical_shape_dim_vec;
      CHECK_EQ(unpack_from_dim_vec.at(in_split_axis) % num_ranks, 0);
      unpack_from_dim_vec[in_split_axis] = unpack_from_dim_vec.at(in_split_axis) / num_ranks;
      CHECK_EQ(unpack_from_dim_vec.at(out_split_axis) % num_ranks, 0);
      unpack_from_dim_vec[out_split_axis] = unpack_from_dim_vec.at(out_split_axis) / num_ranks;
      unpack_from_dim_vec.insert(unpack_from_dim_vec.begin(), num_ranks);
      const Shape unpack_from_shape(unpack_from_dim_vec);
      DimVector transpose_out_dim_vec;
      std::vector<int32_t> perm;
      FOR_RANGE(int64_t, i, 1, unpack_from_shape.NumAxes()) {
        perm.push_back(i);
        transpose_out_dim_vec.push_back(unpack_from_shape.At(i));
      }
      perm.insert(perm.begin() + in_split_axis, 0);
      transpose_out_dim_vec.insert(transpose_out_dim_vec.begin() + in_split_axis,
                                   unpack_from_shape.At(0));
      const Shape transpose_out_shape(transpose_out_dim_vec);
      NewKernelUtil<DeviceType::kGPU>::Transpose(
          ctx->device_ctx(), unpack_from_shape.NumAxes(), unpack_from_shape, transpose_out_shape,
          perm, unpack_from_shape.elem_cnt(), reinterpret_cast<const T*>(unpack_from_ptr),
          out->mut_dptr<T>());
    }
  };
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_EAGER_NCCL_S2S_KERNEL(dtype)                                           \
  REGISTER_USER_KERNEL("eager_nccl_s2s")                                                \
      .SetCreateFn<EagerNcclS2SKernel<dtype>>()                                         \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                               \
                       & (user_op::HobDataType("in", 0) == GetDataType<dtype>::value)   \
                       & (user_op::HobDataType("out", 0) == GetDataType<dtype>::value)) \
      .SetInferTmpSizeFn(InferEagerNcclS2SKernelTmpBufferSize);

REGISTER_EAGER_NCCL_S2S_KERNEL(int8_t)
REGISTER_EAGER_NCCL_S2S_KERNEL(int32_t)
REGISTER_EAGER_NCCL_S2S_KERNEL(int64_t)
REGISTER_EAGER_NCCL_S2S_KERNEL(float)
REGISTER_EAGER_NCCL_S2S_KERNEL(double)
REGISTER_EAGER_NCCL_S2S_KERNEL(float16)
}  // namespace oneflow
