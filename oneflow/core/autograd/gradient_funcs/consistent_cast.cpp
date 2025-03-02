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
#include "oneflow/core/framework/op_interpreter/op_interpreter_util.h"
#include "oneflow/core/framework/op_expr.h"
#include "oneflow/core/framework/op_expr_helper.h"
#include "oneflow/core/framework/nd_sbp.h"
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter_mgr.h"
#include "oneflow/core/framework/tensor_rpc_util.h"
#include "oneflow/core/common/decorator.h"

namespace oneflow {
namespace one {

namespace {

Maybe<Tensor> CalcBoxingOutput(const std::shared_ptr<Tensor>& input, Symbol<cfg::NdSbp> out_nd_sbp,
                               Symbol<ParallelDesc> out_parallel_desc) {
  const auto* mgr = Global<EagerBoxingInterpreterManager>::Get();
  // Eager boxing
  const auto& in_nd_sbp = JUST(input->nd_sbp());
  const auto& in_parallel_desc = JUST(input->parallel_desc());
  const auto& boxing_interpreter = JUST(
      mgr->GetEagerBoxingInterpreter(in_nd_sbp, out_nd_sbp, in_parallel_desc, out_parallel_desc));
  const auto& output = JUST(boxing_interpreter->Interpret(input, in_nd_sbp, out_nd_sbp,
                                                          in_parallel_desc, out_parallel_desc));
  return output;
}

static constexpr auto* RecursiveGetBoxingOutput =
    DECORATE(&CalcBoxingOutput, CheckConsistentTensorMeta);

}  // namespace

struct CastConsistentCaptureState : public AutoGradCaptureState {
  Symbol<ParallelDesc> parallel_desc;
  Symbol<cfg::NdSbp> nd_sbp;
  std::shared_ptr<const Shape> shape;
  Symbol<DType> dtype;
};

class CastToConsistent : public OpExprGradFunction<CastConsistentCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override {
    const auto* fw_op_expr = dynamic_cast<const CastToConsistentOpExpr*>(&op);
    CHECK_NOTNULL_OR_RETURN(fw_op_expr);
    const std::string& op_name = fw_op_expr->op_name();
    grad_op_ = JUST(one::CastFromConsistentOpExpr::New(GradientOpName(op_name)));
    return Maybe<void>::Ok();
  }

  Maybe<void> Capture(CastConsistentCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs,
                      const OpExprInterpContext& interp_ctx) const override {
    ctx->parallel_desc = JUST(interp_ctx.parallel_desc.value());
    ctx->nd_sbp = JUST(GetDualNdSbp(JUST(interp_ctx.nd_sbp.value())));
    return Maybe<void>::Ok();
  }

  Maybe<void> Apply(const CastConsistentCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override {
    CHECK_EQ_OR_RETURN(out_grads.size(), 1);
    std::shared_ptr<Tensor> out_grad = out_grads.at(0);
    CHECK_OR_RETURN(out_grad->is_consistent());
    {
      Symbol<cfg::NdSbp> nd_sbp_constraint = ctx->nd_sbp;
      Symbol<ParallelDesc> parallel_desc_constraint = ctx->parallel_desc;
      out_grad =
          JUST(RecursiveGetBoxingOutput(out_grad, nd_sbp_constraint, parallel_desc_constraint));
    }
    in_grads->at(0) = JUST(OpInterpUtil::Dispatch<Tensor>(*grad_op_, {out_grad}));
    return Maybe<void>::Ok();
  }

 private:
  std::shared_ptr<OpExpr> grad_op_;
};

REGISTER_OP_EXPR_GRAD_FUNCTION("cast_to_consistent", CastToConsistent);

class CastFromConsistent : public OpExprGradFunction<CastConsistentCaptureState> {
 public:
  Maybe<void> Init(const OpExpr& op) override {
    const auto* fw_op_expr = dynamic_cast<const CastFromConsistentOpExpr*>(&op);
    CHECK_NOTNULL_OR_RETURN(fw_op_expr);
    const std::string& op_name = fw_op_expr->op_name();
    grad_op_ = JUST(one::CastToConsistentOpExpr::New(GradientOpName(op_name)));
    return Maybe<void>::Ok();
  }

  Maybe<void> Capture(CastConsistentCaptureState* ctx, const TensorTuple& inputs,
                      const TensorTuple& outputs, const AttrMap& attrs) const override {
    const auto& input = inputs.at(0);
    CHECK_OR_RETURN(input->is_consistent());
    ctx->parallel_desc = JUST(input->parallel_desc());
    ctx->nd_sbp = JUST(input->nd_sbp());
    ctx->shape = input->shape();
    ctx->dtype = input->dtype();
    return Maybe<void>::Ok();
  }

  Maybe<void> Apply(const CastConsistentCaptureState* ctx, const TensorTuple& out_grads,
                    TensorTuple* in_grads) const override {
    const auto& dual_nd_sbp = JUST(GetDualNdSbp(ctx->nd_sbp));
    MutableAttrMap attrs;
    JUST(attrs.SetAttr<Shape>("shape", *ctx->shape));
    JUST(attrs.SetAttr<DataType>("dtype", ctx->dtype->data_type()));
    in_grads->at(0) = JUST(OpInterpUtil::Dispatch<Tensor>(
        *grad_op_, {out_grads.at(0)}, OpExprInterpContext(attrs, ctx->parallel_desc, dual_nd_sbp)));
    return Maybe<void>::Ok();
  }

 private:
  std::shared_ptr<OpExpr> grad_op_;
};

REGISTER_OP_EXPR_GRAD_FUNCTION("cast_from_consistent", CastFromConsistent);

}  // namespace one
}  // namespace oneflow
