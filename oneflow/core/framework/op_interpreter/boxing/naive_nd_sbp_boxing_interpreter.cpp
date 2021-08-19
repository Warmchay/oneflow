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
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter_mgr.h"
#include "oneflow/core/framework/op_interpreter/boxing/naive_nd_sbp_boxing_interpreter.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/placement_sbp_util.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/id_util.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/operator/operator.h"

namespace oneflow {

namespace {

Maybe<one::OpExpr> MakeToConsistentOpExpr() {
  std::shared_ptr<one::OpExpr> op_expr =
      JUST(one::CastToConsistentOpExpr::New(*JUST(UniqueStr("cast_to_consistent"))));
  return op_expr;
}

static constexpr auto* GetLocalToConsistentOpExpr = DECORATE(&MakeToConsistentOpExpr, ThreadLocal);

Maybe<one::Tensor> ReinterpterConsistentTensor(const std::shared_ptr<one::Tensor>& tensor,
                                               const Shape& shape,
                                               Symbol<ParallelDesc> parallel_desc,
                                               Symbol<cfg::NdSbp> nd_sbp) {
  const auto& op = JUST(GetLocalToConsistentOpExpr());
  MutableAttrMap attrs;
  JUST(attrs.SetAttr<Shape>("shape", shape));
  JUST(attrs.SetAttr<DataType>("dtype", tensor->dtype()->data_type()));
  const auto& x = JUST(tensor->cur_rank_phy_tensor());
  return JUST(one::OpInterpUtil::Dispatch<one::Tensor>(
      *op, {x}, one::OpExprInterpContext(attrs, parallel_desc, nd_sbp)));
}

Maybe<one::Tensor> Apply1DBoxing(const std::shared_ptr<one::Tensor>& input,
                                 Symbol<cfg::NdSbp> in_nd_sbp, Symbol<cfg::NdSbp> out_nd_sbp,
                                 Symbol<ParallelDesc> in_parallel_desc,
                                 Symbol<ParallelDesc> out_parallel_desc) {
  const auto& boxing_interpreter =
      JUST(Global<EagerBoxingInterpreterManager>::Get()->GetEagerBoxingInterpreter(
          in_nd_sbp, out_nd_sbp, in_parallel_desc, out_parallel_desc));
  return JUST(boxing_interpreter->Interpret(input, in_nd_sbp, out_nd_sbp, in_parallel_desc,
                                            out_parallel_desc));
}

}  // namespace

Maybe<one::Tensor> NaiveNdSbpBoxingInterpreter::InterpretImpl(
    const std::shared_ptr<one::Tensor>& input, Symbol<cfg::NdSbp> in_nd_sbp,
    Symbol<cfg::NdSbp> out_nd_sbp, Symbol<ParallelDesc> in_parallel_desc,
    Symbol<ParallelDesc> out_parallel_desc) const {
  CHECK_OR_RETURN(in_parallel_desc == out_parallel_desc);
  const auto& tensor_meta = JUST(input->consistent_tensor_meta());
  const auto& naive_transformations =
      JUST(DecomposeIntoNaiveTransformations(tensor_meta, out_nd_sbp));
  std::shared_ptr<one::Tensor> tensor = input;
  for (const auto& naive_transformation : *naive_transformations) {
    const auto& sub_tensor_meta = naive_transformation.consistent_tensor_meta;
    tensor = JUST(ReinterpterConsistentTensor(tensor, sub_tensor_meta->shape(),
                                              sub_tensor_meta->parallel_desc(),
                                              sub_tensor_meta->nd_sbp()));
    tensor =
        JUST(Apply1DBoxing(tensor, sub_tensor_meta->nd_sbp(), naive_transformation.dst_nd_sbp,
                           sub_tensor_meta->parallel_desc(), sub_tensor_meta->parallel_desc()));
  }
  return JUST(ReinterpterConsistentTensor(tensor, *input->shape(), out_parallel_desc, out_nd_sbp));
}

}  // namespace oneflow
