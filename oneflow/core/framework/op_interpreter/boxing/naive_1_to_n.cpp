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
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/framework/nd_sbp.h"
#include "oneflow/core/framework/device.h"
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter_util.h"
#include "oneflow/core/framework/op_interpreter/boxing/eager_boxing_interpreter.h"
#include "oneflow/core/functional/functional.h"
#include "oneflow/core/common/decorator.h"

namespace oneflow {

namespace {

Maybe<void> RawCheckNaive1ToP(Symbol<PlacedNdSbp> in, Symbol<PlacedNdSbp> out) {
  CHECK_EQ_OR_RETURN(in->placement()->parallel_num(), 1);
  CHECK_OR_RETURN(EagerBoxingInterpreterUtil::IsAllPartialSumNdSbp(out->nd_sbp()));
  CHECK_OR_RETURN(out->placement()->Bigger(*in->placement()));
  return Maybe<void>::Ok();
}

static constexpr auto* CheckNaive1ToP = DECORATE(&RawCheckNaive1ToP, ThreadLocal);

}  // namespace

Maybe<one::Tensor> Naive1ToP(const std::shared_ptr<one::Tensor>& tensor, Symbol<PlacedNdSbp> in,
                             Symbol<PlacedNdSbp> out) {
  const auto& tensor_nd_sbp = JUST(tensor->nd_sbp());
  CHECK_OR_RETURN(tensor_nd_sbp == in->nd_sbp());
  const auto& tensor_placement = JUST(tensor->parallel_desc());
  CHECK_OR_RETURN(tensor_placement == in->placement());

  int64_t root = JUST(tensor_placement->MachineId4ParallelId(0));
  std::shared_ptr<one::Tensor> local_tensor = JUST(tensor->cur_rank_phy_tensor());
  const auto& out_parallel_id = JUST(GetParallelId4CurrentProcessCtx(out->placement()));
  if (root == GlobalProcessCtx::Rank() || !out_parallel_id->has_value()) {
    // do nothing
  } else {
    const std::string& device_type = Device::Type4DeviceTag(tensor_placement->device_tag());
    local_tensor = JUST(one::functional::Constant(*tensor->shape(), 0, tensor->dtype(),
                                                  JUST(Device::New(device_type))));
  }
  return JUST(one::functional::LocalToConsistent(local_tensor, out->placement(),
                                                 *JUST(GetSbpList(out->nd_sbp())), *tensor->shape(),
                                                 tensor->dtype()));
}

COMMAND(RegisterBoxingFunction("naive-1-to-p", CheckNaive1ToP, &Naive1ToP));

}  // namespace oneflow
