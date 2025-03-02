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
#ifndef ONEFLOW_USER_KERNELS_DISTRIBUTIONS_UNIFORM_KERNEL_H_
#define ONEFLOW_USER_KERNELS_DISTRIBUTIONS_UNIFORM_KERNEL_H_

#include "oneflow/core/framework/framework.h"
#include "oneflow/user/kernels/distributions/uniform_distribution.h"

namespace oneflow {

class UniformKernelState : public user_op::OpKernelState {
 public:
  explicit UniformKernelState(const std::shared_ptr<one::Generator>& generator)
      : generator_(generator) {}

  const std::shared_ptr<one::Generator>& generator() const { return generator_; }

 private:
  std::shared_ptr<one::Generator> generator_;
};

namespace {

template<DeviceType device_type, typename T>
class UniformKernel final : public user_op::OpKernel {
 public:
  UniformKernel() = default;
  ~UniformKernel() = default;

  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(
      user_op::KernelInitContext* ctx) const override {
    const auto& generator = CHECK_JUST(one::MakeAutoGenerator());
    generator->set_current_seed(ctx->Attr<int64_t>("seed"));
    return std::make_shared<UniformKernelState>(generator);
  }

 private:
  void Compute(user_op::KernelComputeContext* ctx, user_op::OpKernelState* state) const override {
    user_op::Tensor* out = ctx->Tensor4ArgNameAndIndex("out", 0);
    const double low = ctx->Attr<double>("low");
    const double high = ctx->Attr<double>("high");
    int64_t elem_cnt = out->shape().elem_cnt();
    T* out_dptr = out->mut_dptr<T>();
    auto* uniform_state = dynamic_cast<UniformKernelState*>(state);
    CHECK_NOTNULL(uniform_state);
    const auto& generator = uniform_state->generator();
    CHECK_NOTNULL(generator);
    UniformDistribution<device_type, T> distribution(static_cast<T>(low), static_cast<T>(high));
    distribution(ctx->device_ctx(), elem_cnt, out_dptr, generator);
  }
  bool AlwaysComputeWhenAllOutputsEmpty() const override { return false; }
};

}  // namespace
}  // namespace oneflow

#endif  // ONEFLOW_USER_KERNELS_DISTRIBUTIONS_UNIFORM_KERNEL_H_
