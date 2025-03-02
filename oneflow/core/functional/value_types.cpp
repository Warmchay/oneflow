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
#include "oneflow/core/functional/value_types.h"

#include "oneflow/core/common/hash_container.h"

namespace oneflow {
namespace one {
namespace functional {

HashMap<ValueType, std::string>* GetValueTypeNameMap() {
  static HashMap<ValueType, std::string> value_type_name_map = {
      {kVOID, "void"},
      {kINT32, "int32"},
      {kUINT32, "unsigned int32"},
      {kINT64, "int64"},
      {kUINT64, "unsigned int64"},
      {kFLOAT, "float"},
      {kDOUBLE, "double"},
      {kBOOL, "bool"},
      {kSTRING, "string"},
      {kINT32_LIST, "int32 list"},
      {kUINT32_LIST, "unsigned int32 list"},
      {kINT64_LIST, "int64 list"},
      {kUINT64_LIST, "unsigned int64 list"},
      {kFLOAT_LIST, "float list"},
      {kDOUBLE_LIST, "double list"},
      {kDOUBLE_LIST, "bool list"},
      {kSTRING_LIST, "string list"},
      {kVOID_MAYBE, "maybe void"},
      {kBOOL_MAYBE, "maybe bool"},
      {kSCALAR, "scalar"},
      {kTENSOR, "tensor"},
      {kTENSOR_REF, "tensor"},
      {kTENSOR_MAYBE, "maybe tensor"},
      {kTENSOR_TUPLE, "tensor tuple"},
      {kTENSOR_TUPLE_REF, "tensor tuple"},
      {kTENSOR_TUPLE_MAYBE, "maybe tensor tuple"},
      {kATTR, "attr"},
      {kATTR_REF, "attr"},
      {kATTR_MAP, "attr map"},
      {kDTYPE, "data type"},
      {kSHAPE, "shape"},
      {kGENERATOR, "generator"},
      {kGENERATOR_REF, "generator"},
      {kGENERATOR_MAYBE, "maybe generator"},
      {kTENSOR_INDEX, "index"},
      {kDEVICE, "device"},
      {kPARALLEL_DESC, "placement"},
      {kSBP_PARALLEL, "sbp"},
      {kSBP_PARALLEL_LIST, "sbp list"},
  };
  return &value_type_name_map;
}

Maybe<const std::string&> ValueTypeName(ValueType type) {
  const auto* type_name_map = GetValueTypeNameMap();
  const auto& it = type_name_map->find(type);
  CHECK_OR_RETURN(it != type_name_map->end()) << "Value type " << type << " has no type name.";
  return it->second;
}

}  // namespace functional
}  // namespace one
}  // namespace oneflow
