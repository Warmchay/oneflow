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
#include "oneflow/core/framework/placement_sbp_util.h"
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter.h"
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter_util.h"
#include "oneflow/core/common/decorator.h"

namespace oneflow {

namespace {

Maybe<void> RawCheckAsymmetricXToB(Symbol<PlacedNdSbp> in, Symbol<PlacedNdSbp> out) {
  CHECK_EQ_OR_RETURN(in->nd_sbp()->sbp_parallel_size(), 1);
  CHECK_EQ_OR_RETURN(out->nd_sbp()->sbp_parallel_size(), 1);
  CHECK_OR_RETURN(EagerBoxingInterpreterUtil::IsAllBroadcastNdSbp(out->nd_sbp()));
  CHECK_OR_RETURN(out->placement()->Bigger(*in->placement()));
  CHECK_OR_RETURN(in->placement()->device_type() == DeviceType::kGPU);
  return Maybe<void>::Ok();
}

static constexpr auto* CheckAsymmetricXToB = DECORATE(&RawCheckAsymmetricXToB, ThreadLocal);

Maybe<Symbol<cfg::NdSbp>> GetBroadcastNdSbp() {
  cfg::NdSbp broadcast_nd_sbp;
  broadcast_nd_sbp.mutable_sbp_parallel()->Add()->mutable_broadcast_parallel();
  return SymbolOf(broadcast_nd_sbp);
}

auto* CachedGetBroadcastNdSbp = DECORATE(&GetBroadcastNdSbp, ThreadLocal);

}  // namespace

Maybe<one::Tensor> AsymmetricXToB(const std::shared_ptr<one::Tensor>& tensor,
                                  Symbol<PlacedNdSbp> in, Symbol<PlacedNdSbp> out) {
  const auto& tensor_nd_sbp = JUST(tensor->nd_sbp());
  CHECK_OR_RETURN(tensor_nd_sbp == in->nd_sbp());
  const auto& tensor_placement = JUST(tensor->parallel_desc());
  CHECK_OR_RETURN(tensor_placement == in->placement());

  Symbol<cfg::NdSbp> broadcast_nd_sbp = JUST(CachedGetBroadcastNdSbp());
  const auto& broadcast_in_placed_nd_sbp =
      JUST(PlacedNdSbp::New(broadcast_nd_sbp, tensor_placement));
  const auto& SymXToBBoxingFunction =
      *JUST(GetBoxingFunction("symmetric-x-to-b", in, broadcast_in_placed_nd_sbp));
  std::shared_ptr<one::Tensor> broadcast_input =
      JUST(SymXToBBoxingFunction(tensor, in, broadcast_in_placed_nd_sbp));

  const auto& AsymBroadcastBoxingFunction =
      *JUST(GetBoxingFunction("asymmetric-broadcast", broadcast_in_placed_nd_sbp, out));
  return AsymBroadcastBoxingFunction(broadcast_input, broadcast_in_placed_nd_sbp, out);
}

COMMAND(RegisterBoxingFunction("asymmetric-x-to-b", CheckAsymmetricXToB, &AsymmetricXToB));

}  // namespace oneflow
