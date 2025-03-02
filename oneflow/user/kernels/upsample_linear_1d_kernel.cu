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
#include "oneflow/core/common/nd_index_offset_helper.h"
#include "oneflow/core/cuda/atomic.cuh"
#include "oneflow/user/kernels/upsample_kernel.h"

namespace oneflow {

namespace {

template<typename T>
__global__ void UpsampleLinear1DForward(const int64_t elem_cnt, const T* in_dptr,
                                        NdIndexOffsetHelper<int64_t, 3> in_helper,
                                        NdIndexOffsetHelper<int64_t, 3> out_helper,
                                        const int in_height, const float scale_factor,
                                        bool align_corners, T* out_dptr) {
  CUDA_1D_KERNEL_LOOP(index, elem_cnt) {
    int64_t n, c, h;
    out_helper.OffsetToNdIndex(index, n, c, h);
    const T h1r = GetLinearInputIndex(h, scale_factor, align_corners);
    const int64_t h1 = h1r;
    const int64_t h1p = (h1 < in_height - 1) ? 1 : 0;
    const T h1lambda = h1r - h1;
    const T h0lambda = static_cast<T>(1.) - h1lambda;
    out_dptr[index] = h0lambda * in_dptr[in_helper.NdIndexToOffset(n, c, h1)]
                      + h1lambda * in_dptr[in_helper.NdIndexToOffset(n, c, h1 + h1p)];
  }
}

template<typename T>
__global__ void UpsampleLinear1DBackward(const int64_t elem_cnt, const T* dy_dptr,
                                         NdIndexOffsetHelper<int64_t, 3> dy_helper,
                                         NdIndexOffsetHelper<int64_t, 3> dx_helper,
                                         const int in_height, const float scale_factor,
                                         bool align_corners, T* dx_dptr) {
  CUDA_1D_KERNEL_LOOP(index, elem_cnt) {
    int64_t n, c, h;
    dy_helper.OffsetToNdIndex(index, n, c, h);
    const T h1r = GetLinearInputIndex(h, scale_factor, align_corners);
    const int64_t h1 = h1r;
    const int64_t h1p = (h1 < in_height - 1) ? 1 : 0;
    const T h1lambda = h1r - h1;
    const T h0lambda = static_cast<T>(1.) - h1lambda;

    cuda::atomic::Add(dx_dptr + dx_helper.NdIndexToOffset(n, c, h1), h0lambda * dy_dptr[index]);
    cuda::atomic::Add(dx_dptr + dx_helper.NdIndexToOffset(n, c, h1 + h1p),
                      h1lambda * dy_dptr[index]);
  }
}

}  // namespace

template<typename T>
class UpsampleLinear1DGPUKernel final : public user_op::OpKernel {
 public:
  UpsampleLinear1DGPUKernel() = default;
  ~UpsampleLinear1DGPUKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x_tensor = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* y_tensor = ctx->Tensor4ArgNameAndIndex("y", 0);
    const float height_scale = ctx->Attr<float>("scale_factor");
    const bool align_corners = ctx->Attr<bool>("align_corners");
    const int64_t elem_cnt = y_tensor->shape().elem_cnt();
    NdIndexOffsetHelper<int64_t, 3> in_helper(x_tensor->shape().At(0), x_tensor->shape().At(1),
                                              x_tensor->shape().At(2));
    NdIndexOffsetHelper<int64_t, 3> out_helper(y_tensor->shape().At(0), y_tensor->shape().At(1),
                                               y_tensor->shape().At(2));
    const int64_t in_height = x_tensor->shape().At(2);
    const int64_t out_height = y_tensor->shape().At(2);
    if (in_height == out_height) {
      Memcpy<DeviceType::kGPU>(
          ctx->device_ctx(), y_tensor->mut_dptr<void>(), x_tensor->dptr<void>(),
          x_tensor->shape().elem_cnt() * GetSizeOfDataType(x_tensor->data_type()));
    } else {
      const T scale_height = GetAreaPixelScale(in_height, out_height, align_corners, height_scale);
      RUN_CUDA_KERNEL((UpsampleLinear1DForward<T>), ctx->device_ctx(), elem_cnt, elem_cnt,
                      x_tensor->dptr<T>(), in_helper, out_helper, in_height, scale_height,
                      align_corners, y_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<typename T>
class UpsampleLinearGrad1DGPUKernel final : public user_op::OpKernel {
 public:
  UpsampleLinearGrad1DGPUKernel() = default;
  ~UpsampleLinearGrad1DGPUKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    user_op::Tensor* dx_tensor = ctx->Tensor4ArgNameAndIndex("dx", 0);
    Memset<DeviceType::kGPU>(ctx->device_ctx(), dx_tensor->mut_dptr<T>(), 0,
                             dx_tensor->shape().elem_cnt() * sizeof(T));
    const user_op::Tensor* dy_tensor = ctx->Tensor4ArgNameAndIndex("dy", 0);
    const float height_scale = ctx->Attr<float>("scale_factor");
    const bool align_corners = ctx->Attr<bool>("align_corners");

    NdIndexOffsetHelper<int64_t, 3> dy_helper(dy_tensor->shape().At(0), dy_tensor->shape().At(1),
                                              dy_tensor->shape().At(2));
    NdIndexOffsetHelper<int64_t, 3> dx_helper(dx_tensor->shape().At(0), dx_tensor->shape().At(1),
                                              dx_tensor->shape().At(2));
    const int64_t elem_cnt = dy_tensor->shape().elem_cnt();
    const int64_t in_height = dx_tensor->shape().At(2);
    const int64_t out_height = dy_tensor->shape().At(2);
    if (in_height == out_height) {
      Memcpy<DeviceType::kGPU>(
          ctx->device_ctx(), dx_tensor->mut_dptr<void>(), dy_tensor->dptr<void>(),
          dy_tensor->shape().elem_cnt() * GetSizeOfDataType(dy_tensor->data_type()));
    } else {
      const T scale_height = GetAreaPixelScale(in_height, out_height, align_corners, height_scale);
      RUN_CUDA_KERNEL((UpsampleLinear1DBackward<T>), ctx->device_ctx(), elem_cnt, elem_cnt,
                      dy_tensor->dptr<T>(), dy_helper, dx_helper, in_height, scale_height,
                      align_corners, dx_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_UPSAMPLELINEAR1D_GPU_KERNEL(dtype)                                    \
  REGISTER_USER_KERNEL("upsample_linear_1d")                                           \
      .SetCreateFn<UpsampleLinear1DGPUKernel<dtype>>()                                 \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                              \
                       & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value)); \
  REGISTER_USER_KERNEL("upsample_linear_1d_grad")                                      \
      .SetCreateFn<UpsampleLinearGrad1DGPUKernel<dtype>>()                             \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                              \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

REGISTER_UPSAMPLELINEAR1D_GPU_KERNEL(float)
REGISTER_UPSAMPLELINEAR1D_GPU_KERNEL(double)

}  // namespace oneflow
