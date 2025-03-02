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
class IdentityKernel final : public Kernel {
 public:
  OF_DISALLOW_COPY_AND_MOVE(IdentityKernel);
  IdentityKernel() = default;
  ~IdentityKernel() = default;

 private:
  void ForwardDataContent(const KernelContext* ctx) const override;
  void ForwardHeader(const KernelContext* ctx) const override;
};

template<DeviceType device_type>
void IdentityKernel<device_type>::ForwardDataContent(const KernelContext* ctx) const {
  ctx->BnInOp2Blob("out")->CopyValidDataContentFrom(ctx->device_ctx(), ctx->BnInOp2Blob("in"));
}

template<DeviceType device_type>
void IdentityKernel<device_type>::ForwardHeader(const KernelContext* ctx) const {
  ctx->BnInOp2Blob("out")->CopyHeaderFrom(ctx->device_ctx(), ctx->BnInOp2Blob("in"));
}

ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kIdentityConf, IdentityKernel);
ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kCopyConf, IdentityKernel);
ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kCastToMirroredConf, IdentityKernel);
ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kCastFromMirroredConf, IdentityKernel);
ADD_DEVICE_TYPE_KERNEL_CREATOR(OperatorConf::kBoxingIdentityConf, IdentityKernel);

}  // namespace oneflow
