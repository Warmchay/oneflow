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
#include "oneflow/core/framework/framework.h"
#include "oneflow/core/kernel/new_kernel_util.h"
#include "oneflow/core/kernel/cuda_graph_support.h"

namespace oneflow {

namespace {

template<DeviceType device_type, typename T>
class ReluKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  ReluKernel() = default;
  ~ReluKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* y = ctx->Tensor4ArgNameAndIndex("out", 0);
    NewKernelUtil<device_type>::Relu(ctx->device_ctx(), x->shape().elem_cnt(), x->dptr<T>(),
                                     y->mut_dptr<T>());
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_RELU_KERNEL(device, dtype)                                                     \
  REGISTER_USER_KERNEL("relu")                                                                  \
      .SetCreateFn<ReluKernel<device, dtype>>()                                                 \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("out", 0) == GetDataType<dtype>::value))         \
      .SetInplaceProposalFn([](const user_op::InferContext&,                                    \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "in", 0, true));                       \
        return Maybe<void>::Ok();                                                               \
      });

REGISTER_RELU_KERNEL(DeviceType::kCPU, float)
REGISTER_RELU_KERNEL(DeviceType::kCPU, double)
#ifdef WITH_CUDA
REGISTER_RELU_KERNEL(DeviceType::kGPU, float)
REGISTER_RELU_KERNEL(DeviceType::kGPU, double)
REGISTER_RELU_KERNEL(DeviceType::kGPU, float16)
#endif

template<DeviceType device_type, typename T>
class ReluGradKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  ReluGradKernel() = default;
  ~ReluGradKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* y_blob = ctx->Tensor4ArgNameAndIndex("y", 0);
    const user_op::Tensor* dy_blob = ctx->Tensor4ArgNameAndIndex("dy", 0);
    user_op::Tensor* dx_blob = ctx->Tensor4ArgNameAndIndex("dx", 0);
    NewKernelUtil<device_type>::ReluBackward(ctx->device_ctx(), y_blob->shape().elem_cnt(),
                                             y_blob->dptr<T>(), y_blob->dptr<T>(),
                                             dy_blob->dptr<T>(), dx_blob->mut_dptr<T>());
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_RELU_GRAD_KERNEL(device, dtype)                                                \
  REGISTER_USER_KERNEL("relu_grad")                                                             \
      .SetCreateFn<ReluGradKernel<device, dtype>>()                                             \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value))          \
      .SetInplaceProposalFn([](const user_op::InferContext&,                                    \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("dx", 0, "dy", 0, true));                        \
        return Maybe<void>::Ok();                                                               \
      });

REGISTER_RELU_GRAD_KERNEL(DeviceType::kCPU, float)
REGISTER_RELU_GRAD_KERNEL(DeviceType::kCPU, double)
#ifdef WITH_CUDA
REGISTER_RELU_GRAD_KERNEL(DeviceType::kGPU, float)
REGISTER_RELU_GRAD_KERNEL(DeviceType::kGPU, double)
REGISTER_RELU_GRAD_KERNEL(DeviceType::kGPU, float16)
#endif

}  // namespace

}  // namespace oneflow
