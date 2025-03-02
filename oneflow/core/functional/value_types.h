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

#ifndef ONEFLOW_CORE_FUNCTIONAL_VALUE_TYPES_H_
#define ONEFLOW_CORE_FUNCTIONAL_VALUE_TYPES_H_

#include <memory>

#include "oneflow/core/common/data_type.pb.h"
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/common/optional.h"
#include "oneflow/core/framework/dtype.h"

namespace oneflow {
class Shape;
class AttrMap;

template<typename T>
class Symbol;

class Device;
class ParallelDesc;

namespace cfg {
class AttrValue;
class SbpParallel;
}  // namespace cfg

namespace one {
class Tensor;
class TensorTuple;
class Generator;

namespace functional {
class Scalar;
class TensorIndex;
}  // namespace functional
}  // namespace one

namespace one {
namespace functional {

enum ValueType {
  kINVALID = 0,
  kVOID,
  kINT32,
  kUINT32,
  kINT64,
  kUINT64,
  kFLOAT,
  kDOUBLE,
  kBOOL,
  kSTRING,

  kINT32_LIST = 50,
  kUINT32_LIST,
  kINT64_LIST,
  kUINT64_LIST,
  kFLOAT_LIST,
  kDOUBLE_LIST,
  kBOOL_LIST,
  kSTRING_LIST,

  kVOID_MAYBE = 100,
  kBOOL_MAYBE,

  kSCALAR = 200,
  kTENSOR,
  kTENSOR_REF,
  kTENSOR_MAYBE,
  kTENSOR_TUPLE,
  kTENSOR_TUPLE_REF,
  kTENSOR_TUPLE_MAYBE,
  kATTR,
  kATTR_REF,
  kATTR_MAP,
  kDTYPE,
  kSHAPE,
  kGENERATOR,
  kGENERATOR_REF,
  kGENERATOR_MAYBE,
  kTENSOR_INDEX,
  kDEVICE,
  kPARALLEL_DESC,
  kSBP_PARALLEL,
  kSBP_PARALLEL_LIST,
};

#define VALUE_TYPE_OF_IMPL(cpp_type, value_type)                                                 \
  template<typename T, typename std::enable_if<std::is_same<T, cpp_type>::value, int>::type = 0> \
  inline ValueType ValueTypeOf() {                                                               \
    return value_type;                                                                           \
  }                                                                                              \
  template<typename T,                                                                           \
           typename std::enable_if<std::is_same<T, Optional<cpp_type>>::value, int>::type = 0>   \
  inline ValueType ValueTypeOf() {                                                               \
    return value_type;                                                                           \
  }

VALUE_TYPE_OF_IMPL(void, kVOID);
VALUE_TYPE_OF_IMPL(int32_t, kINT32);
VALUE_TYPE_OF_IMPL(uint32_t, kUINT32);
VALUE_TYPE_OF_IMPL(int64_t, kINT64);
VALUE_TYPE_OF_IMPL(uint64_t, kUINT64);
VALUE_TYPE_OF_IMPL(float, kFLOAT);
VALUE_TYPE_OF_IMPL(double, kDOUBLE);
VALUE_TYPE_OF_IMPL(bool, kBOOL);
VALUE_TYPE_OF_IMPL(std::string, kSTRING);
VALUE_TYPE_OF_IMPL(std::vector<int32_t>, kINT32_LIST);
VALUE_TYPE_OF_IMPL(std::vector<uint32_t>, kUINT32_LIST);
VALUE_TYPE_OF_IMPL(std::vector<int64_t>, kINT64_LIST);
VALUE_TYPE_OF_IMPL(std::vector<uint64_t>, kUINT64_LIST);
VALUE_TYPE_OF_IMPL(std::vector<float>, kFLOAT_LIST);
VALUE_TYPE_OF_IMPL(std::vector<double>, kDOUBLE_LIST);
VALUE_TYPE_OF_IMPL(std::vector<bool>, kBOOL_LIST);
VALUE_TYPE_OF_IMPL(std::vector<std::string>, kSTRING_LIST);

VALUE_TYPE_OF_IMPL(Maybe<void>, kVOID_MAYBE);
VALUE_TYPE_OF_IMPL(Maybe<bool>, kBOOL_MAYBE);

VALUE_TYPE_OF_IMPL(Scalar, kSCALAR);
VALUE_TYPE_OF_IMPL(one::Tensor, kTENSOR);
VALUE_TYPE_OF_IMPL(std::shared_ptr<one::Tensor>, kTENSOR_REF);
VALUE_TYPE_OF_IMPL(Maybe<one::Tensor>, kTENSOR_MAYBE);
VALUE_TYPE_OF_IMPL(one::TensorTuple, kTENSOR_TUPLE);
VALUE_TYPE_OF_IMPL(std::shared_ptr<one::TensorTuple>, kTENSOR_TUPLE_REF);
VALUE_TYPE_OF_IMPL(Maybe<one::TensorTuple>, kTENSOR_TUPLE_MAYBE);
VALUE_TYPE_OF_IMPL(cfg::AttrValue, kATTR);
VALUE_TYPE_OF_IMPL(std::shared_ptr<cfg::AttrValue>, kATTR_REF);
VALUE_TYPE_OF_IMPL(AttrMap, kATTR_MAP);
VALUE_TYPE_OF_IMPL(Symbol<DType>, kDTYPE);
VALUE_TYPE_OF_IMPL(Shape, kSHAPE);
VALUE_TYPE_OF_IMPL(one::Generator, kGENERATOR);
VALUE_TYPE_OF_IMPL(std::shared_ptr<one::Generator>, kGENERATOR_REF);
VALUE_TYPE_OF_IMPL(Maybe<one::Generator>, kGENERATOR_MAYBE);
VALUE_TYPE_OF_IMPL(TensorIndex, kTENSOR_INDEX);
VALUE_TYPE_OF_IMPL(Symbol<Device>, kDEVICE);
VALUE_TYPE_OF_IMPL(Symbol<ParallelDesc>, kPARALLEL_DESC);
VALUE_TYPE_OF_IMPL(Symbol<cfg::SbpParallel>, kSBP_PARALLEL);
VALUE_TYPE_OF_IMPL(std::vector<Symbol<cfg::SbpParallel>>, kSBP_PARALLEL_LIST);

#undef VALUE_TYPE_OF_IMPL

Maybe<const std::string&> ValueTypeName(ValueType type);

}  // namespace functional
}  // namespace one
}  // namespace oneflow

namespace std {
template<>
struct hash<oneflow::one::functional::ValueType> {
  std::size_t operator()(oneflow::one::functional::ValueType v) const noexcept { return v; }
};
}  // namespace std

#endif  // ONEFLOW_CORE_FUNCTIONAL_VALUE_TYPES_H_
