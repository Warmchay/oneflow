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
#ifndef ONEFLOW_CORE_PRIMITIVE_MEMCPY_H_
#define ONEFLOW_CORE_PRIMITIVE_MEMCPY_H_

#include "oneflow/core/primitive/include/primitive.h"

namespace oneflow {

namespace primitive {

enum MemcpyKind {
  kAuto = 0,
  kHtoD,
  kDtoH,
  kDtoD,
};

class Memcpy : public Primitive {
 public:
  OF_DISALLOW_COPY_AND_MOVE(Memcpy);
  Memcpy() = default;
  ~Memcpy() override = default;

  virtual void Launch(StreamContext* stream_ctx, void* dst, const void* src, size_t count) = 0;
};

class MemcpyFactory : public Factory<Memcpy> {
 public:
  OF_DISALLOW_COPY_AND_MOVE(MemcpyFactory);
  MemcpyFactory() = default;
  ~MemcpyFactory() override = default;

  virtual std::unique_ptr<Memcpy> New(MemcpyKind kind) = 0;
};

}  // namespace primitive

}  // namespace oneflow

#endif  // ONEFLOW_CORE_PRIMITIVE_MEMCPY_H_
