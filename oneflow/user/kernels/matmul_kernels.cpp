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
#include "oneflow/core/framework/config_def.h"
#include "oneflow/core/kernel/cuda_graph_support.h"

namespace oneflow {

namespace {

std::tuple<int32_t, int32_t, int32_t> CalcMNK(const ShapeView& a_shape, const ShapeView& out_shape,
                                              CBLAS_TRANSPOSE transpose_a) {
  int32_t num_axes = a_shape.NumAxes();
  int m = out_shape.At(num_axes - 2);
  int n = out_shape.At(num_axes - 1);
  int k = transpose_a == CblasTrans ? a_shape.At(num_axes - 2) : a_shape.At(num_axes - 1);
  return std::make_tuple(m, n, k);
}

}  // namespace

template<DeviceType device_type, typename T>
class MatmulFloatingKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  MatmulFloatingKernel() = default;
  ~MatmulFloatingKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    CBLAS_TRANSPOSE trans_a = ctx->Attr<bool>("transpose_a") ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE trans_b = ctx->Attr<bool>("transpose_b") ? CblasTrans : CblasNoTrans;
    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(2, a->shape().NumAxes());

    int32_t m = 0, n = 0, k = 0;
    std::tie(m, n, k) = CalcMNK(a->shape(), out->shape(), trans_a);
    const double alpha = ctx->Attr<double>("alpha");
    double beta;
    if (ctx->has_input("_add_to_output", 0)) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->data_type(), out->data_type());
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<device_type>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
      beta = 1.0;
    } else {
      beta = 0.0;
    }
    NewKernelUtil<device_type>::OFGemm(ctx->device_ctx(), trans_a, trans_b, m, n, k, alpha,
                                       a->dptr<T>(), b->dptr<T>(), beta, out->mut_dptr<T>());
  }
};

#define REGISTER_MATMUL_KERNEL(device, dtype)                                                   \
  REGISTER_USER_KERNEL("matmul")                                                                \
      .SetCreateFn<MatmulFloatingKernel<device, dtype>>()                                       \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("a", 0) == GetDataType<dtype>::value))           \
      .SetInplaceProposalFn([](const user_op::InferContext& ctx,                                \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        if (ctx.has_input("_add_to_output", 0)) {                                               \
          OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));         \
        }                                                                                       \
        return Maybe<void>::Ok();                                                               \
      });

REGISTER_MATMUL_KERNEL(DeviceType::kCPU, float);
REGISTER_MATMUL_KERNEL(DeviceType::kCPU, double);
#ifdef WITH_CUDA
REGISTER_MATMUL_KERNEL(DeviceType::kGPU, float);
REGISTER_MATMUL_KERNEL(DeviceType::kGPU, double);
#endif

#ifdef WITH_CUDA
class MatmulGpuHalfKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  MatmulGpuHalfKernel() = default;
  ~MatmulGpuHalfKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    CBLAS_TRANSPOSE trans_a = ctx->Attr<bool>("transpose_a") ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE trans_b = ctx->Attr<bool>("transpose_b") ? CblasTrans : CblasNoTrans;
    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    CHECK_EQ(2, a->shape().NumAxes());

    int32_t m = 0, n = 0, k = 0;
    std::tie(m, n, k) = CalcMNK(a->shape(), out->shape(), trans_a);
    bool has_add_to_output = ctx->has_input("_add_to_output", 0);
    if (has_add_to_output) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->data_type(), out->data_type());
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<DeviceType::kGPU>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
    }
    const double alpha = ctx->Attr<double>("alpha");
    const double beta = has_add_to_output ? 1.0 : 0.0;
    NewKernelUtil<DeviceType::kGPU>::OFGemm(ctx->device_ctx(), trans_a, trans_b, m, n, k, alpha,
                                            a->dptr<float16>(), b->dptr<float16>(), beta,
                                            out->mut_dptr<float16>());
  }
};

#endif

#ifdef WITH_CUDA
REGISTER_USER_KERNEL("matmul")
    .SetCreateFn<MatmulGpuHalfKernel>()
    .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")
                     & (user_op::HobDataType("a", 0) == DataType::kFloat16))
    .SetInplaceProposalFn([](const user_op::InferContext& ctx,
                             user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> {
      if (ctx.has_input("_add_to_output", 0)) {
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));
      }
      return Maybe<void>::Ok();
    });
#endif

template<DeviceType device_type, typename T>
class BatchMatmulFloatingKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  BatchMatmulFloatingKernel() = default;
  ~BatchMatmulFloatingKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    CBLAS_TRANSPOSE trans_a = ctx->Attr<bool>("transpose_a") ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE trans_b = ctx->Attr<bool>("transpose_b") ? CblasTrans : CblasNoTrans;
    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    int32_t num_axes = a->shape().NumAxes();
    CHECK_GT(num_axes, 2);

    int32_t m = 0, n = 0, k = 0;
    std::tie(m, n, k) = CalcMNK(a->shape(), out->shape(), trans_a);
    const double alpha = ctx->Attr<double>("alpha");
    double beta;
    if (ctx->has_input("_add_to_output", 0)) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->data_type(), out->data_type());
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<device_type>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
      beta = 1.0;
    } else {
      beta = 0.0;
    }
    size_t batch_size = a->shape().Count(0, num_axes - 2);
    NewKernelUtil<device_type>::OFBatchedGemm(ctx->device_ctx(), trans_a, trans_b, batch_size, m, n,
                                              k, alpha, a->dptr<T>(), b->dptr<T>(), beta,
                                              out->mut_dptr<T>());
  }
};

#define REGISTER_BATCH_MATMUL_KERNEL(device, dtype)                                             \
  REGISTER_USER_KERNEL("batch_matmul")                                                          \
      .SetCreateFn<BatchMatmulFloatingKernel<device, dtype>>()                                  \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("a", 0) == GetDataType<dtype>::value))           \
      .SetInplaceProposalFn([](const user_op::InferContext& ctx,                                \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        if (ctx.has_input("_add_to_output", 0)) {                                               \
          OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));         \
        }                                                                                       \
        return Maybe<void>::Ok();                                                               \
      });

REGISTER_BATCH_MATMUL_KERNEL(DeviceType::kCPU, float);
REGISTER_BATCH_MATMUL_KERNEL(DeviceType::kCPU, double);
#ifdef WITH_CUDA
REGISTER_BATCH_MATMUL_KERNEL(DeviceType::kGPU, float);
REGISTER_BATCH_MATMUL_KERNEL(DeviceType::kGPU, double);
#endif

#ifdef WITH_CUDA
class BatchMatmulGpuHalfKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  BatchMatmulGpuHalfKernel() = default;
  ~BatchMatmulGpuHalfKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    CBLAS_TRANSPOSE trans_a = ctx->Attr<bool>("transpose_a") ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE trans_b = ctx->Attr<bool>("transpose_b") ? CblasTrans : CblasNoTrans;
    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    int32_t num_axes = a->shape().NumAxes();
    CHECK_GT(num_axes, 2);

    int32_t m = 0, n = 0, k = 0;
    std::tie(m, n, k) = CalcMNK(a->shape(), out->shape(), trans_a);
    bool has_add_to_output = ctx->has_input("_add_to_output", 0);
    if (has_add_to_output) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->data_type(), out->data_type());
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<DeviceType::kGPU>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
    }
    size_t batch_size = a->shape().Count(0, num_axes - 2);
    const double alpha = ctx->Attr<double>("alpha");
    const double beta = has_add_to_output ? 1.0 : 0.0;
    NewKernelUtil<DeviceType::kGPU>::OFBatchedGemm(
        ctx->device_ctx(), trans_a, trans_b, batch_size, m, n, k, alpha, a->dptr<float16>(),
        b->dptr<float16>(), beta, out->mut_dptr<float16>());
  }
};

REGISTER_USER_KERNEL("batch_matmul")
    .SetCreateFn<BatchMatmulGpuHalfKernel>()
    .SetIsMatchedHob((user_op::HobDeviceTag() == "gpu")
                     & (user_op::HobDataType("a", 0) == DataType::kFloat16))
    .SetInplaceProposalFn([](const user_op::InferContext& ctx,
                             user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> {
      if (ctx.has_input("_add_to_output", 0)) {
        OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));
      }
      return Maybe<void>::Ok();
    });

#endif

template<DeviceType device_type, typename T>
class BroadcastMatmulKernel final : public user_op::OpKernel, public user_op::CudaGraphSupport {
 public:
  BroadcastMatmulKernel() = default;
  ~BroadcastMatmulKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    double alpha = ctx->Attr<double>("alpha");
    bool transpose_a = ctx->Attr<bool>("transpose_a");
    bool transpose_b = ctx->Attr<bool>("transpose_b");
    CHECK(!transpose_a);

    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);

    double beta = 0.0;
    if (ctx->has_input("_add_to_output", 0)) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<device_type>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
      beta = 1.0;
    }

    CHECK_EQ(b->shape().NumAxes(), 2);
    CHECK_GT(a->shape().NumAxes(), b->shape().NumAxes());
    int64_t m = a->shape().Count(0, a->shape().NumAxes() - 1);
    int64_t k = a->shape().At(a->shape().NumAxes() - 1);
    int64_t n = -1;
    if (!transpose_b) {
      n = b->shape().At(1);
      CHECK_EQ(k, b->shape().At(0));
    } else {
      n = b->shape().At(0);
      CHECK_EQ(k, b->shape().At(1));
    }

    CBLAS_TRANSPOSE trans_a = transpose_a ? CblasTrans : CblasNoTrans;
    CBLAS_TRANSPOSE trans_b = transpose_b ? CblasTrans : CblasNoTrans;
    NewKernelUtil<device_type>::OFGemm(ctx->device_ctx(), trans_a, trans_b, m, n, k, alpha,
                                       a->dptr<T>(), b->dptr<T>(), beta, out->mut_dptr<T>());
  }
};

template<DeviceType device_type, typename T>
class BroadcastMatmulGradBKernel final : public user_op::OpKernel,
                                         public user_op::CudaGraphSupport {
 public:
  BroadcastMatmulGradBKernel() = default;
  ~BroadcastMatmulGradBKernel() = default;

  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }

 private:
  void Compute(user_op::KernelComputeContext* ctx) const override {
    double alpha = ctx->Attr<double>("alpha");
    const user_op::Tensor* a = ctx->Tensor4ArgNameAndIndex("a", 0);
    const user_op::Tensor* b = ctx->Tensor4ArgNameAndIndex("b", 0);
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);

    double beta = 0.0;
    if (ctx->has_input("_add_to_output", 0)) {
      const user_op::Tensor* add_to_output = ctx->Tensor4ArgNameAndIndex("_add_to_output", 0);
      CHECK_EQ(add_to_output->shape(), out->shape());
      Memcpy<device_type>(
          ctx->device_ctx(), out->mut_dptr<void>(), add_to_output->dptr<void>(),
          add_to_output->shape().elem_cnt() * GetSizeOfDataType(add_to_output->data_type()));
      beta = 1.0;
    }

    CHECK_EQ(a->shape().NumAxes(), b->shape().NumAxes());
    int64_t k = a->shape().Count(0, a->shape().NumAxes() - 1);
    CHECK_EQ(b->shape().Count(0, b->shape().NumAxes() - 1), k);
    int64_t m = a->shape().At(a->shape().NumAxes() - 1);
    int64_t n = b->shape().At(b->shape().NumAxes() - 1);

    NewKernelUtil<device_type>::OFGemm(ctx->device_ctx(), CblasTrans, CblasNoTrans, m, n, k, alpha,
                                       a->dptr<T>(), b->dptr<T>(), beta, out->mut_dptr<T>());
  }
};

#define REGISTER_BROADCAST_MATMUL_KERNEL(device, dtype)                                         \
  REGISTER_USER_KERNEL("broadcast_matmul")                                                      \
      .SetCreateFn<BroadcastMatmulKernel<device, dtype>>()                                      \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("a", 0) == GetDataType<dtype>::value))           \
      .SetInplaceProposalFn([](const user_op::InferContext& ctx,                                \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        if (ctx.has_input("_add_to_output", 0)) {                                               \
          OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));         \
        }                                                                                       \
        return Maybe<void>::Ok();                                                               \
      })

REGISTER_BROADCAST_MATMUL_KERNEL(DeviceType::kCPU, float);
REGISTER_BROADCAST_MATMUL_KERNEL(DeviceType::kCPU, double);
#ifdef WITH_CUDA
REGISTER_BROADCAST_MATMUL_KERNEL(DeviceType::kGPU, float);
REGISTER_BROADCAST_MATMUL_KERNEL(DeviceType::kGPU, double);
REGISTER_BROADCAST_MATMUL_KERNEL(DeviceType::kGPU, float16);
#endif

#define REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(device, dtype)                                  \
  REGISTER_USER_KERNEL("broadcast_matmul_grad_b")                                               \
      .SetCreateFn<BroadcastMatmulGradBKernel<device, dtype>>()                                 \
      .SetIsMatchedHob((user_op::HobDeviceTag() == device)                                      \
                       & (user_op::HobDataType("a", 0) == GetDataType<dtype>::value))           \
      .SetInplaceProposalFn([](const user_op::InferContext& ctx,                                \
                               user_op::AddInplaceArgPair AddInplaceArgPairFn) -> Maybe<void> { \
        if (ctx.has_input("_add_to_output", 0)) {                                               \
          OF_RETURN_IF_ERROR(AddInplaceArgPairFn("out", 0, "_add_to_output", 0, true));         \
        }                                                                                       \
        return Maybe<void>::Ok();                                                               \
      })

REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(DeviceType::kCPU, float);
REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(DeviceType::kCPU, double);
#ifdef WITH_CUDA
REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(DeviceType::kGPU, float);
REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(DeviceType::kGPU, double);
REGISTER_BROADCAST_MATMUL_GRAD_B_KERNEL(DeviceType::kGPU, float16);
#endif

}  // namespace oneflow
