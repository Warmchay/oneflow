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
#include "oneflow/core/framework/attr_map.h"
#include "oneflow/core/framework/op_expr_grad_function.h"
#include "oneflow/core/framework/op_builder.h"
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/op_expr_helper.h"
#include "oneflow/core/functional/functional.h"

namespace oneflow {
namespace one {

namespace {

struct AvgPoolingCaptureState : public AutoGradCaptureState {
  bool requires_grad;
  size_t input_index;
  size_t output_index;

  std::string data_format;
  std::vector<int32_t> padding;
  std::vector<int32_t> kernel_size;
  std::vector<int32_t> stride;
  bool ceil_mode;
  bool count_include_pad;
  int64_t divisor_override;
};

class AvgPoolingNdGrad : public OpExprGradFunction<AvgPoolingCaptureState> {
 public:
  virtual ~AvgPoolingNdGrad() = default;
  Maybe<void> Init(const OpExpr& op) override;
  Maybe<void> Capture(AvgPoolingCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override;
  Maybe<void> Apply(const AvgPoolingCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override;

 private:
  AttrMap base_attrs_;
};

Maybe<void> AvgPoolingNdGrad::Init(const OpExpr& op) {
  const auto* fw_op_expr = dynamic_cast<const UserOpExpr*>(&op);
  CHECK_NOTNULL_OR_RETURN(fw_op_expr);
  base_attrs_ = MakeAttrMapFromUserOpConf(fw_op_expr->proto());
  return Maybe<void>::Ok();
}

Maybe<void> AvgPoolingNdGrad::Capture(AvgPoolingCaptureState* ctx, const TensorTuple& inputs,
                                      const TensorTuple& outputs, const AttrMap& attrs) const {
  ctx->requires_grad = inputs.at(0)->requires_grad();
  if (!ctx->requires_grad) { return Maybe<void>::Ok(); }

  ctx->input_index = ctx->SaveTensorForBackward(inputs.at(0));
  ctx->output_index = ctx->SaveTensorForBackward(outputs.at(0));

  ComposedAttrMap composed_attrs(attrs, base_attrs_);
  ctx->data_format = JUST(composed_attrs.GetAttr<std::string>("data_format"));
  ctx->padding = JUST(composed_attrs.GetAttr<std::vector<int32_t>>("padding"));
  ctx->kernel_size = JUST(composed_attrs.GetAttr<std::vector<int32_t>>("kernel_size"));
  ctx->stride = JUST(composed_attrs.GetAttr<std::vector<int32_t>>("stride"));
  ctx->ceil_mode = JUST(composed_attrs.GetAttr<bool>("ceil_mode"));
  ctx->count_include_pad = JUST(composed_attrs.GetAttr<bool>("count_include_pad"));
  ctx->divisor_override = JUST(composed_attrs.GetAttr<int64_t>("divisor_override"));

  return Maybe<void>::Ok();
}

Maybe<void> AvgPoolingNdGrad::Apply(const AvgPoolingCaptureState* ctx, const TensorTuple& out_grads,
                                    TensorTuple* in_grads) const {
  if (!ctx->requires_grad) { return Maybe<void>::Ok(); }
  CHECK_EQ_OR_RETURN(out_grads.size(), 1);

  int32_t ndims = ctx->kernel_size.size();
  const auto& input = ctx->SavedTensors().at(ctx->input_index);
  const auto& output = ctx->SavedTensors().at(ctx->output_index);

  in_grads->resize(1);
  in_grads->at(0) = JUST(functional::AvgPoolingNdGrad(
      input, output, out_grads.at(0), ndims, ctx->data_format, ctx->padding, ctx->kernel_size,
      ctx->stride, ctx->ceil_mode, ctx->count_include_pad, ctx->divisor_override));

  return Maybe<void>::Ok();
}

}  // namespace

REGISTER_OP_EXPR_GRAD_FUNCTION("avgpool_1d", AvgPoolingNdGrad);
REGISTER_OP_EXPR_GRAD_FUNCTION("avgpool_2d", AvgPoolingNdGrad);
REGISTER_OP_EXPR_GRAD_FUNCTION("avgpool_3d", AvgPoolingNdGrad);

}  // namespace one
}  // namespace oneflow
