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
#include "oneflow/core/framework/attr_map.h"

namespace oneflow {
namespace one {

struct BiasAddCaptureState : public AutoGradCaptureState {
  bool input_requires_grad;
  bool bias_requires_grad;
  int32_t axis;
};

class BiasAdd : public OpExprGradFunction<BiasAddCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override {
    const auto* fw_op_expr = dynamic_cast<const UserOpExpr*>(&op);
    CHECK_NOTNULL_OR_RETURN(fw_op_expr);
    base_attrs_ = MakeAttrMapFromUserOpConf(fw_op_expr->proto());
    const std::string& op_name = fw_op_expr->op_name();
    backward_input_op_ = JUST(op_expr_helper::IdentityOp(GradientOpName(op_name + "_input")));
    backward_bias_op_ = JUST(
        op_expr_helper::ReduceSumOp({0}, /*keepdims=*/false, GradientOpName(op_name + "_bias")));
    return Maybe<void>::Ok();
  }

  Maybe<void> Capture(BiasAddCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override {
    CHECK_EQ_OR_RETURN(inputs.size(), 2);
    ctx->input_requires_grad = inputs.at(0)->requires_grad();
    ctx->bias_requires_grad = inputs.at(1)->requires_grad();
    ComposedAttrMap composed_attrs(attrs, base_attrs_);
    ctx->axis = JUST(composed_attrs.GetAttr<int32_t>("axis"));
    return Maybe<void>::Ok();
  }

  Maybe<void> Apply(const BiasAddCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override {
    const int64_t num_axes = out_grads.at(0)->shape()->NumAxes();
    in_grads->resize(2);
    if (ctx->bias_requires_grad) {
      std::vector<int32_t> reduce_axes_vec;
      reduce_axes_vec.reserve(num_axes);
      for (int i = 0; i < num_axes; ++i) {
        if (i != ctx->axis) { reduce_axes_vec.push_back(i); }
      }
      MutableAttrMap attrs;
      JUST(attrs.SetAttr<std::vector<int32_t>>("axis", reduce_axes_vec));
      in_grads->at(1) =
          JUST(OpInterpUtil::Dispatch<Tensor>(*backward_bias_op_, {out_grads.at(0)}, attrs));
    }
    if (ctx->input_requires_grad) {
      in_grads->at(0) =
          JUST(OpInterpUtil::Dispatch<Tensor>(*backward_input_op_, {out_grads.at(0)}));
    }
    return Maybe<void>::Ok();
  }

 private:
  AttrMap base_attrs_;
  std::shared_ptr<OpExpr> backward_input_op_;
  std::shared_ptr<OpExpr> backward_bias_op_;
};

REGISTER_OP_EXPR_GRAD_FUNCTION("bias_add", BiasAdd);

}  // namespace one
}  // namespace oneflow
