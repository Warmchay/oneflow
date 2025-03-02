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
#include "oneflow/user/kernels/math_unary_elementwise_func.h"
#include "oneflow/core/kernel/cuda_graph_support.h"

namespace oneflow {

namespace {

template<template<typename> class UnaryFunctor, typename T>
__global__ void MathUnaryElementwiseForwardGpu(const int n, const T* x, T* y) {
  CUDA_1D_KERNEL_LOOP(i, n) { y[i] = UnaryFunctor<T>::Forward(x[i]); }
}

template<template<typename> class UnaryFunctor, typename T>
__global__ void MathUnaryElementwiseBackwardGpu(const int n, const T* x, const T* dy, T* dx) {
  CUDA_1D_KERNEL_LOOP(i, n) { dx[i] = UnaryFunctor<T>::Backward(x[i], dy[i]); }
}

}  // namespace

template<template<typename> class UnaryFunctor, typename T>
class MathUnaryElementwiseGpuKernel final : public user_op::OpKernel,
                                            public user_op::CudaGraphSupport {
 public:
  MathUnaryElementwiseGpuKernel() = default;
  ~MathUnaryElementwiseGpuKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* tensor_x = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* tensor_y = ctx->Tensor4ArgNameAndIndex("y", 0);
    const T* x = tensor_x->dptr<T>();
    T* y = tensor_y->mut_dptr<T>();
    int64_t n = tensor_x->shape().elem_cnt();
    CHECK_LE(n, GetMaxVal<int32_t>() / 2);
    if (n == 0) { return; }
    MathUnaryElementwiseForwardGpu<UnaryFunctor, T>
        <<<BlocksNum4ThreadsNum(n), kCudaThreadsNumPerBlock, 0, ctx->device_ctx()->cuda_stream()>>>(
            n, x, y);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<template<typename> class UnaryFunctor, typename T>
class MathUnaryElementwiseGradGpuKernel final : public user_op::OpKernel,
                                                public user_op::CudaGraphSupport {
 public:
  MathUnaryElementwiseGradGpuKernel() = default;
  ~MathUnaryElementwiseGradGpuKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* tensor_x = ctx->Tensor4ArgNameAndIndex("x", 0);
    const user_op::Tensor* tensor_dy = ctx->Tensor4ArgNameAndIndex("dy", 0);
    user_op::Tensor* tensor_dx = ctx->Tensor4ArgNameAndIndex("dx", 0);

    const T* x = tensor_x->dptr<T>();
    const T* dy = tensor_dy->dptr<T>();
    T* dx = tensor_dx->mut_dptr<T>();
    int64_t n = tensor_x->shape().elem_cnt();
    CHECK_LE(n, GetMaxVal<int32_t>() / 2);
    if (n == 0) { return; }
    MathUnaryElementwiseBackwardGpu<UnaryFunctor, T>
        <<<BlocksNum4ThreadsNum(n), kCudaThreadsNumPerBlock, 0, ctx->device_ctx()->cuda_stream()>>>(
            n, x, dy, dx);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_MATH_UNARY_ELEMENTWISE_GPU_KERNEL_AND_GRAD(math_type_pair, data_type_pair)        \
  REGISTER_USER_KERNEL(OF_PP_PAIR_FIRST(math_type_pair))                                           \
      .SetCreateFn<                                                                                \
          MathUnaryElementwiseGpuKernel<OF_PP_CAT(OF_PP_PAIR_SECOND(math_type_pair), Functor),     \
                                        OF_PP_PAIR_FIRST(data_type_pair)>>()                       \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                                          \
                       & (user_op::HobDataType("x", 0) == OF_PP_PAIR_SECOND(data_type_pair))       \
                       & (user_op::HobDataType("y", 0) == OF_PP_PAIR_SECOND(data_type_pair)));     \
                                                                                                   \
  REGISTER_USER_KERNEL((std::string("") + OF_PP_PAIR_FIRST(math_type_pair) + "_grad"))             \
      .SetCreateFn<                                                                                \
          MathUnaryElementwiseGradGpuKernel<OF_PP_CAT(OF_PP_PAIR_SECOND(math_type_pair), Functor), \
                                            OF_PP_PAIR_FIRST(data_type_pair)>>()                   \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                                          \
                       & (user_op::HobDataType("x", 0) == OF_PP_PAIR_SECOND(data_type_pair)));

OF_PP_SEQ_PRODUCT_FOR_EACH_TUPLE(REGISTER_MATH_UNARY_ELEMENTWISE_GPU_KERNEL_AND_GRAD,
                                 MATH_UNARY_ELEMENTWISE_FUNC_SEQ, FLOATING_DATA_TYPE_SEQ)

template<template<typename> class UnaryFunctor>
class MathUnaryElementwiseGpuHalfKernel final : public user_op::OpKernel,
                                                public user_op::CudaGraphSupport {
 public:
  MathUnaryElementwiseGpuHalfKernel() = default;
  ~MathUnaryElementwiseGpuHalfKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* tensor_x = ctx->Tensor4ArgNameAndIndex("x", 0);
    user_op::Tensor* tensor_y = ctx->Tensor4ArgNameAndIndex("y", 0);
    const half* x = reinterpret_cast<const half*>(tensor_x->dptr<float16>());
    half* y = reinterpret_cast<half*>(tensor_y->mut_dptr<float16>());
    int64_t n = tensor_x->shape().elem_cnt();
    CHECK_LE(n, GetMaxVal<int32_t>() / 2);
    if (n == 0) { return; }
    MathUnaryElementwiseForwardGpu<UnaryFunctor, half>
        <<<BlocksNum4ThreadsNum(n), kCudaThreadsNumPerBlock, 0, ctx->device_ctx()->cuda_stream()>>>(
            n, x, y);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

template<template<typename> class UnaryFunctor>
class MathUnaryElementwiseGradGpuHalfKernel final : public user_op::OpKernel,
                                                    public user_op::CudaGraphSupport {
 public:
  MathUnaryElementwiseGradGpuHalfKernel() = default;
  ~MathUnaryElementwiseGradGpuHalfKernel() = default;

 private:
  using user_op::OpKernel::Compute;
  void Compute(user_op::KernelComputeContext* ctx) const override {
    const user_op::Tensor* tensor_x = ctx->Tensor4ArgNameAndIndex("x", 0);
    const user_op::Tensor* tensor_dy = ctx->Tensor4ArgNameAndIndex("dy", 0);
    user_op::Tensor* tensor_dx = ctx->Tensor4ArgNameAndIndex("dx", 0);

    const half* x = reinterpret_cast<const half*>(tensor_x->dptr<float16>());
    const half* dy = reinterpret_cast<const half*>(tensor_dy->dptr<float16>());
    half* dx = reinterpret_cast<half*>(tensor_dx->mut_dptr<float16>());
    int64_t n = tensor_x->shape().elem_cnt();
    CHECK_LE(n, GetMaxVal<int32_t>() / 2);
    if (n == 0) { return; }
    MathUnaryElementwiseBackwardGpu<UnaryFunctor, half>
        <<<BlocksNum4ThreadsNum(n), kCudaThreadsNumPerBlock, 0, ctx->device_ctx()->cuda_stream()>>>(
            n, x, dy, dx);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

#define REGISTER_MATH_UNARY_ELEMENTWISE_GPU_HALF_KERNEL_AND_GRAD(math_type_str, math_func_prefix) \
  REGISTER_USER_KERNEL(math_type_str)                                                             \
      .SetCreateFn<MathUnaryElementwiseGpuHalfKernel<OF_PP_CAT(math_func_prefix, Functor)>>()     \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                                         \
                       & (user_op::HobDataType("x", 0) == DataType::kFloat16)                     \
                       & (user_op::HobDataType("y", 0) == DataType::kFloat16));                   \
                                                                                                  \
  REGISTER_USER_KERNEL((std::string("") + math_type_str + "_grad"))                               \
      .SetCreateFn<MathUnaryElementwiseGradGpuHalfKernel<OF_PP_CAT(math_func_prefix, Functor)>>() \
      .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")                                         \
                       & (user_op::HobDataType("x", 0) == DataType::kFloat16));

OF_PP_FOR_EACH_TUPLE(REGISTER_MATH_UNARY_ELEMENTWISE_GPU_HALF_KERNEL_AND_GRAD,
                     MATH_UNARY_ELEMENTWISE_FUNC_SEQ)

}  // namespace oneflow
