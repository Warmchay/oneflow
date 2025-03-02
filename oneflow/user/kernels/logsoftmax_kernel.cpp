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
#include "oneflow/user/kernels/softmax_kernel_util.h"
#include "oneflow/user/kernels/logsoftmax_kernel_util.h"

namespace oneflow {

namespace {

template<DeviceType device_type, typename T>
class LogSoftmaxKernel final : public user_op::OpKernel {
 public:
  LogSoftmaxKernel() = default;
  ~LogSoftmaxKernel() override = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* in = ctx->Tensor4ArgNameAndIndex("in", 0);
    user_op::Tensor* prob = ctx->Tensor4ArgNameAndIndex("prob", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    const int64_t num_classes = in->shape().At(in->shape().NumAxes() - 1);
    const int64_t num_instances = in->shape().Count(0, in->shape().NumAxes() - 1);
    user_op::Tensor* tmp_buffer = ctx->Tensor4ArgNameAndIndex("tmp_buffer", 0);
    const size_t temp_storage_bytes = tmp_buffer->shape().elem_cnt();

    LogSoftmaxKernelUtil<device_type, T>::ComputeOut(
        ctx->device_ctx(), num_instances, num_classes, in->dptr<T>(), prob->mut_dptr<T>(),
        out->mut_dptr<T>(), tmp_buffer->mut_dptr(), temp_storage_bytes);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<DeviceType device_type, typename T>
user_op::InferTmpSizeFn GenFwInferTmpSizeFn() {
  return [](user_op::InferContext* ctx) {
    const Shape& in_shape = ctx->InputShape("in", 0);
    const int64_t num_classes = in_shape.At(in_shape.NumAxes() - 1);
    const int64_t num_instances = in_shape.Count(0, in_shape.NumAxes() - 1);
    return LogSoftmaxKernelUtil<device_type, T>::GetComputeProbTempStorageSizeInBytes(num_instances,
                                                                                      num_classes);
  };
}

#define REGISTER_LOGSOFTMAX_KERNEL(device, dtype)                                        \
  REGISTER_USER_KERNEL("logsoftmax")                                                     \
      .SetCreateFn<LogSoftmaxKernel<device, dtype>>()                                    \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                               \
                       & (user_op::HobDataType("out", 0) == GetDataType<dtype>::value)   \
                       & (user_op::HobDataType("prob", 0) == GetDataType<dtype>::value)) \
      .SetInferTmpSizeFn(GenFwInferTmpSizeFn<device, dtype>());

REGISTER_LOGSOFTMAX_KERNEL(DeviceType::kCPU, float)
REGISTER_LOGSOFTMAX_KERNEL(DeviceType::kCPU, double)

template<DeviceType device_type, typename T>
class LogSoftmaxGradKernel final : public user_op::OpKernel {
 public:
  LogSoftmaxGradKernel() = default;
  ~LogSoftmaxGradKernel() override = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* prob = ctx->Tensor4ArgNameAndIndex("prob", 0);
    const user_op::Tensor* dy = ctx->Tensor4ArgNameAndIndex("dy", 0);
    user_op::Tensor* dx = ctx->Tensor4ArgNameAndIndex("dx", 0);

    const int64_t num_classes = prob->shape().At(prob->shape().NumAxes() - 1);
    const int64_t num_instances = prob->shape().elem_cnt() / num_classes;

    user_op::Tensor* tmp_buffer = ctx->Tensor4ArgNameAndIndex("tmp_buffer", 0);
    const size_t temp_storage_bytes = tmp_buffer->shape().elem_cnt();

    LogSoftmaxKernelUtil<device_type, T>::ComputeDiff(
        ctx->device_ctx(), num_instances, num_classes, dy->dptr<T>(), prob->dptr<T>(),
        dx->mut_dptr<T>(), tmp_buffer->mut_dptr(), temp_storage_bytes);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<DeviceType device_type, typename T>
user_op::InferTmpSizeFn GenBwInferTmpSizeFn() {
  return [](user_op::InferContext* ctx) {
    const Shape& dy_shape = ctx->InputShape("dy", 0);
    const int64_t num_classes = dy_shape.At(dy_shape.NumAxes() - 1);
    const int64_t num_instances = dy_shape.Count(0, dy_shape.NumAxes() - 1);
    return LogSoftmaxKernelUtil<device_type, T>::GetComputeDiffTempStorageSizeInBytes(num_instances,
                                                                                      num_classes);
  };
}

#define REGISTER_LOGSOFTMAX_GRAD_KERNEL(device, dtype)                                 \
  REGISTER_USER_KERNEL("logsoftmax_grad")                                              \
      .SetCreateFn<LogSoftmaxGradKernel<device, dtype>>()                              \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                             \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value)) \
      .SetInferTmpSizeFn(GenBwInferTmpSizeFn<device, dtype>());

REGISTER_LOGSOFTMAX_GRAD_KERNEL(DeviceType::kCPU, float)
REGISTER_LOGSOFTMAX_GRAD_KERNEL(DeviceType::kCPU, double)

}  // namespace

}  // namespace oneflow
