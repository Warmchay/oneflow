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
#ifndef ONEFLOW_CORE_KERNEL_KERNEL_OBSERVER_H_
#define ONEFLOW_CORE_KERNEL_KERNEL_OBSERVER_H_

#include "oneflow/core/common/util.h"

namespace oneflow {

class Kernel;
class KernelContext;
class Blob;

class KernelObserver {
 public:
  OF_DISALLOW_COPY_AND_MOVE(KernelObserver);
  KernelObserver() = default;
  virtual ~KernelObserver() = default;

  virtual void WillForward(const KernelContext* kernel_ctx, const Kernel* kernel) {}
  virtual void DidForward(const KernelContext* kernel_ctx, const Kernel* kernel) {}

  virtual void WillForwardHeader(const KernelContext* kernel_ctx, const Kernel* kernel) {}
  virtual void DidForwardHeader(const KernelContext* kernel_ctx, const Kernel* kernel) {}

  virtual void WillForwardDataContent(const KernelContext* kernel_ctx, const Kernel* kernel) {}
  virtual void DidForwardDataContent(const KernelContext* kernel_ctx, const Kernel* kernel) {}
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_KERNEL_KERNEL_OBSERVER_H_
