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
#include "oneflow/core/kernel/kernel.h"
#include "oneflow/core/kernel/kernel_context.h"

namespace oneflow {

template<DeviceType device_type>
class BoxingZerosKernel final : public Kernel {
 public:
  OF_DISALLOW_COPY_AND_MOVE(BoxingZerosKernel);
  BoxingZerosKernel() = default;
  ~BoxingZerosKernel() override = default;

 private:
  void ForwardDataContent(const KernelContext* ctx) const override;
};

template<DeviceType device_type>
void BoxingZerosKernel<device_type>::ForwardDataContent(const KernelContext* ctx) const {
  Blob* out = ctx->BnInOp2Blob("out");
  Memset<device_type>(ctx->device_ctx(), out->mut_dptr(), 0, out->ByteSizeOfBlobBody());
}

ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kBoxingZerosConf, BoxingZerosKernel);

}  // namespace oneflow
