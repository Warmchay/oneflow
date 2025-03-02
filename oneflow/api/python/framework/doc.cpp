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

#include "oneflow/api/python/of_api_registry.h"
#include "oneflow/api/python/framework/throw.h"

namespace py = pybind11;

namespace oneflow {

py::object AddFunctionDoc(py::object f, const std::string& doc_string) {
  static std::vector<std::string> all_doc_strings;
  all_doc_strings.emplace_back(doc_string);
  const char* doc_str = all_doc_strings.back().c_str();
  PyObject* obj = f.ptr();
  if (PyCFunction_Check(obj)) {
    auto* f = (PyCFunctionObject*)obj;
    if (f->m_ml->ml_doc) {
      THROW(RuntimeError) << "function " << f->m_ml->ml_name << " already has a docstring.";
    }
    f->m_ml->ml_doc = doc_str;
  } else {
    THROW(RuntimeError) << "function is " << Py_TYPE(obj)->tp_name << ", not a valid PyCFunction.";
  }
  f.inc_ref();
  return f;
}

}  // namespace oneflow

ONEFLOW_API_PYBIND11_MODULE("", m) { m.def("add_doc", &oneflow::AddFunctionDoc); }
