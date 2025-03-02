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

namespace oneflow {

namespace {

const int32_t NDIMS = 16;
struct SIZE_V {
  int32_t val[NDIMS];
};

struct VIS {
  bool val[NDIMS] = {false};
};

template<typename T>
__global__ void FlipGpuForward(const int32_t element, const int64_t total_dims,
                               const SIZE_V stride_contiguous_v, const SIZE_V sizes_v,
                               const VIS vis, SIZE_V strides_v, const T* in_dptr, T* out_dptr) {
  CUDA_1D_KERNEL_LOOP(i, element) {
    int32_t cur_indices = i;
    int32_t rem = 0;
    int32_t dst_offset = 0;
    for (int32_t d = 0; d < total_dims; d++) {
      int32_t temp = cur_indices;
      cur_indices = cur_indices / stride_contiguous_v.val[d];
      rem = temp - cur_indices * stride_contiguous_v.val[d];
      dst_offset += vis.val[d] ? (sizes_v.val[d] - 1 - cur_indices) * strides_v.val[d]
                               : cur_indices * strides_v.val[d];
      cur_indices = rem;
    }
    out_dptr[i] = in_dptr[dst_offset];
  }
}

}  // namespace

template<typename T>
class FlipGpuKernel final : public user_op::OpKernel {
 public:
  FlipGpuKernel() = default;
  ~FlipGpuKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x_tensor = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* y_tensor = ctx->Tensor4ArgNameAndIndex("y", 0);
    const int32_t elem_cnt = y_tensor->shape().elem_cnt();

    const int32_t total_dims = y_tensor->shape().NumAxes();

    std::vector<int32_t> dims = ctx->Attr<std::vector<int32_t>>("dims");
    VIS vis;
    for (auto x : dims) { vis.val[x] = true; }

    SIZE_V sizes_v;
    for (int32_t i = 0; i < total_dims; i++) { sizes_v.val[i] = y_tensor->shape().At(i); }

    // TODO(bbuf) delete strides caluculate, after tensor strides supported
    SIZE_V strides_v;
    strides_v.val[total_dims - 1] = 1;
    for (int32_t i = total_dims - 2; i >= 0; i--) {
      strides_v.val[i] = strides_v.val[i + 1] * y_tensor->shape().At(i + 1);
    }

    SIZE_V stride_contiguous_v;

    for (int32_t i = total_dims - 1; i >= 0; i--) {
      if (i == total_dims - 1) {
        stride_contiguous_v.val[i] = 1;
      } else {
        stride_contiguous_v.val[i] =
            std::max<int32_t>(x_tensor->shape().At(i + 1), 1) * stride_contiguous_v.val[i + 1];
      }
    }
    RUN_CUDA_KERNEL((FlipGpuForward<T>), ctx->device_ctx(), elem_cnt, elem_cnt, total_dims,
                    stride_contiguous_v, sizes_v, vis, strides_v, x_tensor->dptr<T>(),
                    y_tensor->mut_dptr<T>());
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<typename T>
class FlipGrad1DGpuKernel final : public user_op::OpKernel {
 public:
  FlipGrad1DGpuKernel() = default;
  ~FlipGrad1DGpuKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    user_op::Tensor* dx_tensor = ctx->Tensor4ArgNameAndIndex("dx", 0);
    Memset<DeviceType::kGPU>(ctx->device_ctx(), dx_tensor->mut_dptr<T>(), 0,
                             dx_tensor->shape().elem_cnt() * sizeof(T));
    const user_op::Tensor* dy_tensor = ctx->Tensor4ArgNameAndIndex("dy", 0);
    Memcpy<DeviceType::kGPU>(
        ctx->device_ctx(), dx_tensor->mut_dptr<void>(), dy_tensor->dptr<void>(),
        dy_tensor->shape().elem_cnt() * GetSizeOfDataType(dy_tensor->data_type()));
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_FLIP_GPU_KERNEL(dtype)                                             \
  REGISTER_USER_KERNEL("flip").SetCreateFn<FlipGpuKernel<dtype>>().SetIsMatchedHob( \
      (user_op::HobDeviceTag() == "gpu")                                            \
      & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value));               \
  REGISTER_USER_KERNEL("flip_grad")                                                 \
      .SetCreateFn<FlipGrad1DGpuKernel<dtype>>()                                    \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                           \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

REGISTER_FLIP_GPU_KERNEL(float)
REGISTER_FLIP_GPU_KERNEL(double)
REGISTER_FLIP_GPU_KERNEL(int)

}  // namespace oneflow
