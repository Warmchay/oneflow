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
#include "oneflow/user/kernels/upsample_kernel.h"

namespace oneflow {

namespace {

template<typename T>
static void UpsampleNearest1DForward(const int64_t elem_cnt, const T* in_dptr,
                                     NdIndexOffsetHelper<int64_t, 3> in_helper,
                                     NdIndexOffsetHelper<int64_t, 3> out_helper,
                                     const int64_t in_height, const float scale_factor,
                                     T* out_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, h;
    out_helper.OffsetToNdIndex(index, n, c, h);
    const int64_t in_h = GetNearestInputIndex(h, scale_factor, in_height);
    out_dptr[index] = in_dptr[in_helper.NdIndexToOffset(n, c, in_h)];
  }
}

template<typename T>
static void UpsampleNearest1DBackward(const int64_t elem_cnt, const T* dy_dptr,
                                      NdIndexOffsetHelper<int64_t, 3> dy_helper,
                                      NdIndexOffsetHelper<int64_t, 3> dx_helper,
                                      const int64_t in_height, const float scale_factor,
                                      T* dx_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, h;
    dy_helper.OffsetToNdIndex(index, n, c, h);
    const int64_t dx_h = GetNearestInputIndex(h, scale_factor, in_height);
    *(dx_dptr + dx_helper.NdIndexToOffset(n, c, dx_h)) += dy_dptr[index];
  }
}

template<typename T>
static void UpsampleNearest2DForward(const int64_t elem_cnt, const T* in_dptr,
                                     NdIndexOffsetHelper<int64_t, 4> in_helper,
                                     NdIndexOffsetHelper<int64_t, 4> out_helper,
                                     const int64_t in_height, const int64_t in_width,
                                     const float scale_h, const float scale_w, T* out_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, h, w;
    out_helper.OffsetToNdIndex(index, n, c, h, w);
    const int64_t in_h = GetNearestInputIndex(h, scale_h, in_height);
    const int64_t in_w = GetNearestInputIndex(w, scale_w, in_width);
    out_dptr[index] = in_dptr[in_helper.NdIndexToOffset(n, c, in_h, in_w)];
  }
}

template<typename T>
static void UpsampleNearest2DBackward(const int64_t elem_cnt, const T* dy_dptr,
                                      NdIndexOffsetHelper<int64_t, 4> dy_helper,
                                      NdIndexOffsetHelper<int64_t, 4> dx_helper,
                                      const int64_t dx_height, const int64_t dx_width,
                                      const float scale_h, const float scale_w, T* dx_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, h, w;
    dy_helper.OffsetToNdIndex(index, n, c, h, w);
    const int64_t dx_h = GetNearestInputIndex(h, scale_h, dx_height);
    const int64_t dx_w = GetNearestInputIndex(w, scale_w, dx_width);
    *(dx_dptr + dx_helper.NdIndexToOffset(n, c, dx_h, dx_w)) += dy_dptr[index];
  }
}

template<typename T>
static void UpsampleNearest3DForward(const int64_t elem_cnt, const T* in_dptr,
                                     NdIndexOffsetHelper<int64_t, 5> in_helper,
                                     NdIndexOffsetHelper<int64_t, 5> out_helper,
                                     const int64_t in_depth, const int64_t in_height,
                                     const int64_t in_width, const float scale_d,
                                     const float scale_h, const float scale_w, T* out_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, d, h, w;
    out_helper.OffsetToNdIndex(index, n, c, d, h, w);
    const int64_t in_h = GetNearestInputIndex(h, scale_h, in_height);
    const int64_t in_w = GetNearestInputIndex(w, scale_w, in_width);
    const int64_t in_d = GetNearestInputIndex(d, scale_d, in_depth);
    out_dptr[index] = in_dptr[in_helper.NdIndexToOffset(n, c, in_d, in_h, in_w)];
  }
}

template<typename T>
static void UpsampleNearest3DBackward(const int64_t elem_cnt, const T* dy_dptr,
                                      NdIndexOffsetHelper<int64_t, 5> dy_helper,
                                      NdIndexOffsetHelper<int64_t, 5> dx_helper,
                                      const int64_t in_depth, const int64_t in_height,
                                      const int64_t in_width, const float scale_d,
                                      const float scale_h, const float scale_w, T* dx_dptr) {
  for (int64_t index = 0; index < elem_cnt; ++index) {
    int64_t n, c, d, h, w;
    dy_helper.OffsetToNdIndex(index, n, c, d, h, w);
    const int64_t dx_h = GetNearestInputIndex(h, scale_h, in_height);
    const int64_t dx_w = GetNearestInputIndex(w, scale_w, in_width);
    const int64_t in_d = GetNearestInputIndex(d, scale_d, in_depth);
    *(dx_dptr + dx_helper.NdIndexToOffset(n, c, in_d, dx_h, dx_w)) += dy_dptr[index];
  }
}

}  // namespace

template<typename T>
class UpsampleNearest1DCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearest1DCPUKernel() = default;
  ~UpsampleNearest1DCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x_tensor = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* y_tensor = ctx->Tensor4ArgNameAndIndex("y", 0);
    const float height_scale = ctx->Attr<float>("scale_factor");
    const int64_t elem_cnt = y_tensor->shape().elem_cnt();

    const int64_t nbatch = x_tensor->shape().At(0);
    const int64_t channels = x_tensor->shape().At(1);
    const int64_t in_height = x_tensor->shape().At(2);
    const int64_t out_height = y_tensor->shape().At(2);

    if (in_height == out_height) {
      memcpy(y_tensor->mut_dptr<void>(), x_tensor->dptr<void>(),
             sizeof(T) * nbatch * channels * in_height);
    } else {
      NdIndexOffsetHelper<int64_t, 3> in_helper(x_tensor->shape().At(0), x_tensor->shape().At(1),
                                                x_tensor->shape().At(2));
      NdIndexOffsetHelper<int64_t, 3> out_helper(y_tensor->shape().At(0), y_tensor->shape().At(1),
                                                 y_tensor->shape().At(2));
      UpsampleNearest1DForward<T>(elem_cnt, x_tensor->dptr<T>(), in_helper, out_helper,
                                  x_tensor->shape().At(2), 1.f / height_scale,
                                  y_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<typename T>
class UpsampleNearestGrad1DCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearestGrad1DCPUKernel() = default;
  ~UpsampleNearestGrad1DCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    user_op::Tensor* dx_tensor = ctx->Tensor4ArgNameAndIndex("dx", 0);

    Memset<DeviceType::kCPU>(ctx->device_ctx(), dx_tensor->mut_dptr<T>(), 0,
                             dx_tensor->shape().elem_cnt() * sizeof(T));
    const user_op::Tensor* dy_tensor = ctx->Tensor4ArgNameAndIndex("dy", 0);
    const float height_scale = ctx->Attr<float>("scale_factor");

    const int64_t elem_cnt = dy_tensor->shape().elem_cnt();
    const int64_t nbatch = dx_tensor->shape().At(0);
    const int64_t channels = dx_tensor->shape().At(1);
    const int64_t in_height = dx_tensor->shape().At(2);
    const int64_t out_height = dy_tensor->shape().At(2);
    if (in_height == out_height) {
      memcpy(dx_tensor->mut_dptr<void>(), dy_tensor->dptr<void>(),
             sizeof(T) * nbatch * channels * in_height);
    } else {
      NdIndexOffsetHelper<int64_t, 3> dy_helper(dy_tensor->shape().At(0), dy_tensor->shape().At(1),
                                                dy_tensor->shape().At(2));
      NdIndexOffsetHelper<int64_t, 3> dx_helper(dx_tensor->shape().At(0), dx_tensor->shape().At(1),
                                                dx_tensor->shape().At(2));
      UpsampleNearest1DBackward<T>(elem_cnt, dy_tensor->dptr<T>(), dy_helper, dx_helper,
                                   dx_tensor->shape().At(2), 1.f / height_scale,
                                   dx_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_UPSAMPNEAREST1D_CPU_KERNEL(dtype)                                     \
  REGISTER_USER_KERNEL("upsample_nearest_1d")                                          \
      .SetCreateFn<UpsampleNearest1DCPUKernel<dtype>>()                                \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value)); \
  REGISTER_USER_KERNEL("upsample_nearest_1d_grad")                                     \
      .SetCreateFn<UpsampleNearestGrad1DCPUKernel<dtype>>()                            \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

REGISTER_UPSAMPNEAREST1D_CPU_KERNEL(float)
REGISTER_UPSAMPNEAREST1D_CPU_KERNEL(double)

template<typename T>
class UpsampleNearest2DCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearest2DCPUKernel() = default;
  ~UpsampleNearest2DCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x_tensor = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* y_tensor = ctx->Tensor4ArgNameAndIndex("y", 0);

    const int64_t nbatch = x_tensor->shape().At(0);
    const int64_t channels = x_tensor->shape().At(1);
    const int64_t in_height = x_tensor->shape().At(2);
    const int64_t in_width = x_tensor->shape().At(3);
    const int64_t out_height = y_tensor->shape().At(2);
    const int64_t out_width = y_tensor->shape().At(3);

    const float height_scale = ctx->Attr<float>("height_scale");
    const float width_scale = ctx->Attr<float>("width_scale");
    const int64_t elem_cnt = y_tensor->shape().elem_cnt();

    if (in_height == out_height && in_width == out_width) {
      memcpy(y_tensor->mut_dptr<void>(), x_tensor->dptr<void>(),
             sizeof(T) * nbatch * channels * in_height * in_width);
    } else {
      NdIndexOffsetHelper<int64_t, 4> in_helper(x_tensor->shape().At(0), x_tensor->shape().At(1),
                                                x_tensor->shape().At(2), x_tensor->shape().At(3));
      NdIndexOffsetHelper<int64_t, 4> out_helper(y_tensor->shape().At(0), y_tensor->shape().At(1),
                                                 y_tensor->shape().At(2), y_tensor->shape().At(3));
      UpsampleNearest2DForward<T>(elem_cnt, x_tensor->dptr<T>(), in_helper, out_helper,
                                  x_tensor->shape().At(2), x_tensor->shape().At(3),
                                  1.f / height_scale, 1.f / width_scale, y_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<typename T>
class UpsampleNearest2DGradCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearest2DGradCPUKernel() = default;
  ~UpsampleNearest2DGradCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    user_op::Tensor* dx_tensor = ctx->Tensor4ArgNameAndIndex("dx", 0);

    Memset<DeviceType::kCPU>(ctx->device_ctx(), dx_tensor->mut_dptr<T>(), 0,
                             dx_tensor->shape().elem_cnt() * sizeof(T));
    const user_op::Tensor* dy_tensor = ctx->Tensor4ArgNameAndIndex("dy", 0);

    const int64_t nbatch = dx_tensor->shape().At(0);
    const int64_t channels = dx_tensor->shape().At(1);
    const int64_t in_height = dx_tensor->shape().At(2);
    const int64_t in_width = dx_tensor->shape().At(3);
    const int64_t out_height = dy_tensor->shape().At(2);
    const int64_t out_width = dy_tensor->shape().At(3);

    const float height_scale = ctx->Attr<float>("height_scale");
    const float width_scale = ctx->Attr<float>("width_scale");
    const int64_t elem_cnt = dy_tensor->shape().elem_cnt();

    if (in_height == out_height && in_width == out_width) {
      memcpy(dx_tensor->mut_dptr<void>(), dy_tensor->dptr<void>(),
             sizeof(T) * nbatch * channels * in_height * in_width);
    } else {
      NdIndexOffsetHelper<int64_t, 4> dy_helper(dy_tensor->shape().At(0), dy_tensor->shape().At(1),
                                                dy_tensor->shape().At(2), dy_tensor->shape().At(3));
      NdIndexOffsetHelper<int64_t, 4> dx_helper(dx_tensor->shape().At(0), dx_tensor->shape().At(1),
                                                dx_tensor->shape().At(2), dx_tensor->shape().At(3));
      UpsampleNearest2DBackward<T>(elem_cnt, dy_tensor->dptr<T>(), dy_helper, dx_helper,
                                   dx_tensor->shape().At(2), dx_tensor->shape().At(3),
                                   1.f / height_scale, 1.f / width_scale, dx_tensor->mut_dptr<T>());
    }
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_UPSAMPLE_NEAREST_2D_CPU_KERNEL(dtype)                                 \
  REGISTER_USER_KERNEL("upsample_nearest_2d")                                          \
      .SetCreateFn<UpsampleNearest2DCPUKernel<dtype>>()                                \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value)); \
  REGISTER_USER_KERNEL("upsample_nearest_2d_grad")                                     \
      .SetCreateFn<UpsampleNearest2DGradCPUKernel<dtype>>()                            \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

REGISTER_UPSAMPLE_NEAREST_2D_CPU_KERNEL(float)
REGISTER_UPSAMPLE_NEAREST_2D_CPU_KERNEL(double)

template<typename T>
class UpsampleNearest3DCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearest3DCPUKernel() = default;
  ~UpsampleNearest3DCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* x_blob = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* y_blob = ctx->Tensor4ArgNameAndIndex("y", 0);
    const float depth_scale = ctx->Attr<float>("depth_scale");
    const float height_scale = ctx->Attr<float>("height_scale");
    const float width_scale = ctx->Attr<float>("width_scale");
    const int64_t elem_cnt = y_blob->shape().elem_cnt();
    NdIndexOffsetHelper<int64_t, 5> in_helper(x_blob->shape().At(0), x_blob->shape().At(1),
                                              x_blob->shape().At(2), x_blob->shape().At(3),
                                              x_blob->shape().At(4));
    NdIndexOffsetHelper<int64_t, 5> out_helper(y_blob->shape().At(0), y_blob->shape().At(1),
                                               y_blob->shape().At(2), y_blob->shape().At(3),
                                               y_blob->shape().At(4));
    UpsampleNearest3DForward<T>(elem_cnt, x_blob->dptr<T>(), in_helper, out_helper,
                                x_blob->shape().At(2), x_blob->shape().At(3), x_blob->shape().At(4),
                                1.f / depth_scale, 1.f / height_scale, 1.f / width_scale,
                                y_blob->mut_dptr<T>());
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<typename T>
class UpsampleNearestGrad3DCPUKernel final : public user_op::OpKernel {
 public:
  UpsampleNearestGrad3DCPUKernel() = default;
  ~UpsampleNearestGrad3DCPUKernel() = default;

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    user_op::Tensor* dx_blob = ctx->Tensor4ArgNameAndIndex("dx", 0);
    if (dx_blob == nullptr) { return; }
    Memset<DeviceType::kCPU>(ctx->device_ctx(), dx_blob->mut_dptr<T>(), 0,
                             dx_blob->shape().elem_cnt() * sizeof(T));
    const user_op::Tensor* dy_blob = ctx->Tensor4ArgNameAndIndex("dy", 0);
    const float depth_scale = ctx->Attr<float>("depth_scale");
    const float height_scale = ctx->Attr<float>("height_scale");
    const float width_scale = ctx->Attr<float>("width_scale");
    const int64_t elem_cnt = dy_blob->shape().elem_cnt();
    NdIndexOffsetHelper<int64_t, 5> dy_helper(dy_blob->shape().At(0), dy_blob->shape().At(1),
                                              dy_blob->shape().At(2), dy_blob->shape().At(3),
                                              dy_blob->shape().At(4));
    NdIndexOffsetHelper<int64_t, 5> dx_helper(dx_blob->shape().At(0), dx_blob->shape().At(1),
                                              dx_blob->shape().At(2), dx_blob->shape().At(3),
                                              dx_blob->shape().At(4));
    UpsampleNearest3DBackward<T>(elem_cnt, dy_blob->dptr<T>(), dy_helper, dx_helper,
                                 dx_blob->shape().At(2), dx_blob->shape().At(3),
                                 dx_blob->shape().At(4), 1.f / depth_scale, 1.f / height_scale,
                                 1.f / width_scale, dx_blob->mut_dptr<T>());
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_UPSAMPNEAREST3D_CPU_KERNEL(dtype)                                     \
  REGISTER_USER_KERNEL("upsample_nearest_3d")                                          \
      .SetCreateFn<UpsampleNearest3DCPUKernel<dtype>>()                                \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("y", 0) == GetDataType<dtype>::value)); \
  REGISTER_USER_KERNEL("upsample_nearest_3d_grad")                                     \
      .SetCreateFn<UpsampleNearestGrad3DCPUKernel<dtype>>()                            \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "cpu")                              \
                       & (user_op::HobDataType("dx", 0) == GetDataType<dtype>::value));

REGISTER_UPSAMPNEAREST3D_CPU_KERNEL(float)
REGISTER_UPSAMPNEAREST3D_CPU_KERNEL(double)

}  // namespace oneflow
