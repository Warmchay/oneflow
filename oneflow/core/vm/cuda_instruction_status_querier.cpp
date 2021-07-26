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
#if defined(WITH_CUDA) || defined(WITH_HIP)

#include "oneflow/core/vm/cuda_instruction_status_querier.h"
#include "oneflow/core/device/device_context.h"

namespace oneflow {
namespace vm {
#if defined(WITH_CUDA)
bool CudaInstrStatusQuerier::event_completed() const {
  cudaSetDevice(device_id_);
  return cudaEventQuery(event_) == cudaSuccess;
}

void CudaInstrStatusQuerier::SetLaunched(DeviceCtx* device_ctx) {
  cudaSetDevice(device_id_);
  OF_CUDA_CHECK(cudaEventCreateWithFlags(&event_, cudaEventBlockingSync | cudaEventDisableTiming));
  OF_CUDA_CHECK(cudaEventRecord(event_, device_ctx->cuda_stream()));
  launched_ = true;
}
#elif defined(WITH_HIP)
bool CudaInstrStatusQuerier::event_completed() const {
  hipSetDevice(device_id_);
  return hipEventQuery(event_) == hipSuccess;
}

void CudaInstrStatusQuerier::SetLaunched(DeviceCtx* device_ctx) {
  hipSetDevice(device_id_);
  OF_ROCM_CHECK(hipEventCreateWithFlags	(&event_, hipEventBlockingSync | hipEventDisableTiming));
  OF_ROCM_CHECK(hipEventRecord(event_, device_ctx->rocm_stream()));
  launched_ = true;
}
#endif

}  // namespace vm
}  // namespace oneflow

#endif
