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
#include "oneflow/core/framework/id_util.h"
#include "oneflow/core/framework/op_expr_grad_function.h"
#include "oneflow/core/framework/op_builder.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/nd_sbp.h"
#include "oneflow/core/functional/functional.h"
#include "oneflow/core/common/optional.h"

namespace oneflow {
namespace one {

struct ConsistentToConsistentState : public AutoGradCaptureState {
  Symbol<ParallelDesc> parallel_desc;
  Symbol<cfg::NdSbp> nd_sbp;
};

class ConsistentToConsistentGradFunction : public OpExprGradFunction<ConsistentToConsistentState> {
 public:
  Maybe<void> Init(const OpExpr& op) override {
    const auto* fw_op_expr = dynamic_cast<const ConsistentToConsistentOpExpr*>(&op);
    CHECK_NOTNULL_OR_RETURN(fw_op_expr);
    grad_nd_sbp_ = fw_op_expr->grad_nd_sbp();
    return Maybe<void>::Ok();
  }

  Maybe<void> Capture(ConsistentToConsistentState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs,
                      const OpExprInterpContext& interp_ctx) const override {
    CHECK_EQ_OR_RETURN(inputs.size(), 1);
    ctx->parallel_desc = JUST(inputs.at(0)->parallel_desc());
    ctx->nd_sbp = JUST(inputs.at(0)->nd_sbp());
    return Maybe<void>::Ok();
  }

  Maybe<void> Apply(const ConsistentToConsistentState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override {
    CHECK_EQ_OR_RETURN(out_grads.size(), 1);
    const auto& out_grad = out_grads.at(0);
    CHECK_OR_RETURN(out_grad->is_consistent());
    in_grads->resize(1);
    const auto& grad_nd_sbp =
        grad_nd_sbp_.has_value() ? JUST(grad_nd_sbp_.value()) : JUST(out_grad->nd_sbp());
    const auto& grad_sbp_list = JUST(GetSbpList(grad_nd_sbp));
    const auto& grad_grad_sbp_list = JUST(GetSbpList(ctx->nd_sbp));
    in_grads->at(0) = JUST(one::functional::ToConsistent(out_grad, ctx->parallel_desc,
                                                         *grad_sbp_list, *grad_grad_sbp_list));
    return Maybe<void>::Ok();
  }

 private:
  Optional<Symbol<cfg::NdSbp>> grad_nd_sbp_;
};

REGISTER_OP_EXPR_GRAD_FUNCTION("consistent_to_consistent", ConsistentToConsistentGradFunction);

}  // namespace one
}  // namespace oneflow
