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

#include "oneflow/core/framework/op_kernel.h"
#include "oneflow/core/framework/op_kernel_infer_cache.h"
#include "oneflow/core/framework/user_op_tensor.h"
#include "oneflow/core/framework/to_string.h"
#include "oneflow/core/framework/user_op_conf.h"
#include "oneflow/core/framework/user_op_registry_manager.h"
#include "oneflow/core/kernel/eager_kernel.h"
#include "oneflow/core/kernel/kernel.h"
#include "oneflow/core/device/cuda_graph_context.h"

namespace oneflow {

class UserKernelComputeContext;
class UserKernelInferContext;

class UserKernel final : public Kernel {
 public:
  OF_DISALLOW_COPY_AND_MOVE(UserKernel);
  UserKernel() = default;
  ~UserKernel() override;

  void InitUserKernel(DeviceCtx* device_ctx);
  std::shared_ptr<user_op::OpKernelState> CreateOpKernelState(DeviceCtx* device_ctx);
  const std::shared_ptr<user_op::OpKernelState>& GetOpKernelState() const;
  void ForwardUserKernel(const std::function<Blob*(const std::string&)>& BnInOp2Blob,
                         user_op::OpKernelState* opkernel_state) const;
  bool IsCudaGraphSupported() const;

 private:
  void VirtualKernelInit(KernelContext* ctx) override;

  void ForwardDataContent(const KernelContext* ctx) const override;
  void ForwardShape(const KernelContext* ctx) const override;

  bool IsStateless() const override;

  const JobDesc& job_desc() const { return *job_desc_; }

  std::shared_ptr<user_op::OpKernelState> opkernel_state_;
  std::unique_ptr<const user_op::OpKernel> kernel_;
  std::unique_ptr<UserKernelComputeContext> ctx_;
  std::unique_ptr<UserKernelInferContext> infer_ctx_;
  std::unique_ptr<user_op::OpKernelInferCache> infer_cache_;
#ifdef WITH_CUDA_GRAPHS
  std::unique_ptr<CudaGraphContext> cuda_graph_ctx_;
#endif  // WITH_CUDA_GRAPHS
  const JobDesc* job_desc_;
};

}  // namespace oneflow
