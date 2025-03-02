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
#include <pybind11/pybind11.h>
#include <pybind11/operators.h>
#include "oneflow/api/python/of_api_registry.h"
#include "oneflow/core/framework/dtype.h"
namespace py = pybind11;

namespace oneflow {

ONEFLOW_API_PYBIND11_MODULE("", m) {
  py::class_<Symbol<DType>, std::shared_ptr<Symbol<DType>>>(m, "dtype")
      .def_property_readonly("is_signed", [](const Symbol<DType>& d) { return d->is_signed(); })
      .def_property_readonly("is_complex", [](const Symbol<DType>& d) { return d->is_complex(); })
      .def_property_readonly("is_floating_point",
                             [](const Symbol<DType>& d) { return d->is_floating_point(); })
      .def("__str__", [](const Symbol<DType>& d) { return d->name(); })
      .def("__repr__", [](const Symbol<DType>& d) { return d->name(); })
      .def(py::self == py::self)
      .def(py::hash(py::self))
      .def(py::pickle(
          [](const Symbol<DType>& dtype) {  // __getstate__
            return static_cast<int>(dtype->data_type());
          },
          [](int t) {  // __setstate__
            return CHECK_JUST(DType::Get(DataType(t)));
          }))
      .def_property_readonly(
          "bytes", [](const Symbol<DType>& dtype) { return dtype->bytes().GetOrThrow(); });

  m.attr("char") = &CHECK_JUST(DType::Get(DataType::kChar));
  m.attr("float16") = &CHECK_JUST(DType::Get(DataType::kFloat16));
  m.attr("float") = &CHECK_JUST(DType::Get(DataType::kFloat));

  m.attr("float32") = &CHECK_JUST(DType::Get(DataType::kFloat));
  m.attr("double") = &CHECK_JUST(DType::Get(DataType::kDouble));
  m.attr("float64") = &CHECK_JUST(DType::Get(DataType::kDouble));

  m.attr("int8") = &CHECK_JUST(DType::Get(DataType::kInt8));
  m.attr("int32") = &CHECK_JUST(DType::Get(DataType::kInt32));
  m.attr("int64") = &CHECK_JUST(DType::Get(DataType::kInt64));

  m.attr("uint8") = &CHECK_JUST(DType::Get(DataType::kUInt8));
  m.attr("record") = &CHECK_JUST(DType::Get(DataType::kOFRecord));
  m.attr("tensor_buffer") = &CHECK_JUST(DType::Get(DataType::kTensorBuffer));
}

}  // namespace oneflow
