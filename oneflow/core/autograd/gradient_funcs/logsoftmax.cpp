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
#include "oneflow/core/framework/op_expr_grad_function.h"
#include "oneflow/core/framework/op_builder.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/op_expr_helper.h"
#include "oneflow/core/framework/user_op_conf_trait.h"

namespace oneflow {
namespace one {

struct LogSoftmaxCaptureState : public AutoGradCaptureState {
  bool requires_grad;
};

class LogSoftmax : public OpExprGradFunction<LogSoftmaxCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override;
  Maybe<void> Capture(LogSoftmaxCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override;
  Maybe<void> Apply(const LogSoftmaxCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override;

 private:
  AttrMap base_attrs_;
  std::shared_ptr<OpExpr> grad_op_;
};

Maybe<void> LogSoftmax::Init(const OpExpr& op) {
  const auto* fw_op_expr = dynamic_cast<const UserOpExpr*>(&op);
  CHECK_NOTNULL_OR_RETURN(fw_op_expr);
  const std::string& op_name = fw_op_expr->op_name();
  base_attrs_ = MakeAttrMapFromUserOpConf(fw_op_expr->proto());
  grad_op_ = JUST(one::OpBuilder("logsoftmax_grad", GradientOpName(op_name))
                      .Input("prob")
                      .Input("dy")
                      .Output("dx")
                      .Build());
  return Maybe<void>::Ok();
}

Maybe<void> LogSoftmax::Capture(LogSoftmaxCaptureState* ctx, const TensorTuple& inputs,
                                const TensorTuple& outputs, const AttrMap& attrs) const {
  ComposedAttrMap composed_attrs(attrs, base_attrs_);
  CHECK_EQ_OR_RETURN(inputs.size(), 1);
  ctx->requires_grad = inputs.at(0)->requires_grad();

  if (!ctx->requires_grad) return Maybe<void>::Ok();

  ctx->SaveTensorForBackward(outputs.at(1));
  return Maybe<void>::Ok();
}

Maybe<void> LogSoftmax::Apply(const LogSoftmaxCaptureState* ctx, const TensorTuple& out_grads,
                              TensorTuple* in_grads) const {
  if (!ctx->requires_grad) return Maybe<void>::Ok();
  CHECK_EQ_OR_RETURN(out_grads.size(), 2);
  const auto& dy = out_grads.at(0);
  const auto& prob = ctx->SavedTensors().at(0);
  in_grads->resize(1);
  in_grads->at(0) = JUST(OpInterpUtil::Dispatch<Tensor>(*grad_op_, {prob, dy}));
  return Maybe<void>::Ok();
}

REGISTER_OP_EXPR_GRAD_FUNCTION("logsoftmax", LogSoftmax);

}  // namespace one
}  // namespace oneflow
