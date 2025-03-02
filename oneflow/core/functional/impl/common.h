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
#ifndef ONEFLOW_CORE_FUNCTIONAL_IMPL_COMMON_H_
#define ONEFLOW_CORE_FUNCTIONAL_IMPL_COMMON_H_

#include "oneflow/core/framework/tensor.h"

namespace oneflow {
namespace one {
namespace functional {

static constexpr size_t kMaxInputCount = 128;
static constexpr size_t kMaxOutputCount = 128;

bool IsStaticZerosTensor(const std::shared_ptr<Tensor>& x);
bool IsInplaceValid(const std::shared_ptr<Tensor>& x);
bool IsShapeCanExpandTo(const Shape& shape, const Shape& expand_shape);

Maybe<void> CheckInplaceValid(const std::shared_ptr<Tensor>& x);

Maybe<void> CheckShapeCanExpandTo(const Shape& shape, const Shape& expand_shape);

}  // namespace functional
}  // namespace one
}  // namespace oneflow

#endif  // ONEFLOW_CORE_FUNCTIONAL_IMPL_COMMON_H_
