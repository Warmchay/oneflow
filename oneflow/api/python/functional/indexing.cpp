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
#include "oneflow/api/python/functional/indexing.h"

#include <pybind11/pybind11.h>
#include "oneflow/core/eager/eager_blob_object.h"
#include "oneflow/core/register/ofblob.h"
#include "oneflow/core/framework/device.h"
#include "oneflow/core/framework/instructions_builder.h"
#include "oneflow/core/functional/functional.h"

namespace oneflow {
namespace one {
namespace functional {

namespace detail {

Maybe<void> PySliceUnpack(PyObject* object, Py_ssize_t* start, Py_ssize_t* stop, Py_ssize_t* step) {
  PySliceObject* obj = (PySliceObject*)object;
  if (obj->step == Py_None) {
    *step = 1;
  } else {
    CHECK_OR_RETURN(_PyEval_SliceIndex(obj->step, step))
        << "Invalid slice " << JUST(PyStringAsString(PyObject_Repr(object)));
    CHECK_NE_OR_RETURN(*step, 0) << "slice step cannot be zero.";
    if (*step < -PY_SSIZE_T_MAX) *step = -PY_SSIZE_T_MAX;
  }
  if (obj->start == Py_None) {
    *start = *step < 0 ? PY_SSIZE_T_MAX : 0;
  } else {
    CHECK_OR_RETURN(_PyEval_SliceIndex(obj->start, start))
        << "Invalid slice " << JUST(PyStringAsString(PyObject_Repr(object)));
  }
  if (obj->stop == Py_None) {
    *stop = *step < 0 ? PY_SSIZE_T_MIN : PY_SSIZE_T_MAX;
  } else {
    CHECK_OR_RETURN(_PyEval_SliceIndex(obj->stop, stop))
        << "Invalid slice " << JUST(PyStringAsString(PyObject_Repr(object)));
  }
  return Maybe<void>::Ok();
}

Maybe<DataType> InferScalarType(PyObject* object) {
  if (PyLong_Check(object)) {
    return DataType::kInt64;
  } else if (PyBool_Check(object)) {
    return DataType::kUInt8;
  } else if (PySequence_Check(object)) {
    int64_t length = PySequence_Length(object);
    CHECK_GT_OR_RETURN(length, 0) << "Index should not be empty.";
    DataType scalar_type = DataType::kInvalidDataType;
    for (int64_t i = 0; i < length; ++i) {
      PyObjectPtr item(PySequence_GetItem(object, i));
      const auto& item_scalar_type = JUST(InferScalarType(item.get()));
      if (scalar_type != DataType::kInvalidDataType) {
        CHECK_EQ_OR_RETURN(scalar_type, item_scalar_type)
            << "Different scalar types are not allowed.";
      } else {
        scalar_type = item_scalar_type;
      }
    }
    return scalar_type;
  }
  UNIMPLEMENTED_THEN_RETURN() << "Can't infer scalar type of " << Py_TYPE(object)->tp_name;
}

Maybe<void> ParseScalar(PyObject* object, char* data, const DataType& dtype) {
  if (dtype == DataType::kInt64) {
    CHECK_OR_RETURN(PyLong_Check(object)) << "Expected a long value.";
    *(reinterpret_cast<int64_t*>(data)) = PyLong_AsLongLong(object);
    return Maybe<void>::Ok();
  } else if (dtype == DataType::kUInt8) {
    CHECK_OR_RETURN(PyBool_Check(object) || PyLong_Check(object))
        << "Expected a boolean or long value.";
    if (PyBool_Check(object)) {
      *(reinterpret_cast<uint8_t*>(data)) = (object == Py_True);
    } else {
      int64_t value = PyLong_AsLongLong(object);
      CHECK_OR_RETURN(value >= 0 && value <= 255) << "Out of range 0-255.";
      *(reinterpret_cast<uint8_t*>(data)) = value;
    }
    return Maybe<void>::Ok();
  }
  UNIMPLEMENTED_THEN_RETURN() << "Can't parse scalar with data type " << dtype;
}

Maybe<void> RecursiveParseAndAssign(PyObject* object, char* data, const int& ndims, const int& dim,
                                    const ShapeView& shape, const DimVector& strides,
                                    const DataType& dtype) {
  if (dim == ndims) { return ParseScalar(object, data, dtype); }
  auto seq = PyObjectPtr(PySequence_Fast(object, "Expected a sequence."));
  int64_t size = PySequence_Fast_GET_SIZE(seq.get());
  CHECK_EQ_OR_RETURN(size, shape.At(dim)) << "Sequence size is " << size << " at dimemsion " << dim
                                          << ", but expected " << shape.At(dim);
  for (int64_t i = 0; i < size; ++i) {
    PyObject* item = PySequence_Fast_GET_ITEM(seq.get(), i);
    JUST(RecursiveParseAndAssign(item, data, ndims, dim + 1, shape, strides, dtype));
    data += strides.at(dim) * GetSizeOfDataType(dtype);
  }
  return Maybe<void>::Ok();
}

Maybe<void> ParseArrayToBlob(PyObject* object, Blob* blob) {
  const DataType dtype = blob->data_type();
  const int ndims = blob->shape().NumAxes();
  DimVector strides(ndims);
  int64_t size = 1;
  for (int i = ndims - 1; i >= 0; --i) {
    strides[i] = size;
    size *= blob->shape().At(i);
  }
  JUST(RecursiveParseAndAssign(object, blob->mut_dptr<char>(), ndims, 0, blob->shape(), strides,
                               dtype));
  return Maybe<void>::Ok();
}

Maybe<Shape> InferArraySizes(PyObject* object) {
  DimVector sizes;
  PyObject* seq = object;
  PyObjectPtr handle;
  while (PySequence_Check(seq)) {
    int64_t length = PySequence_Length(seq);
    CHECK_GT_OR_RETURN(length, 0) << "Index should not be empty.";
    sizes.push_back(length);
    CHECK_LE_OR_RETURN(sizes.size(), /*MAX_DIMS=*/128)
        << "Too many dimensions " << Py_TYPE(seq)->tp_name;
    if (length == 0) break;
    handle = PyObjectPtr(PySequence_GetItem(seq, 0));
    seq = handle.get();
  }
  return std::make_shared<Shape>(sizes);
}

Maybe<Tensor> ConvertToIndexingTensor(PyObject* object) {
  const DataType dtype = JUST(InferScalarType(object));
  const auto& sizes = JUST(InferArraySizes(object));
  const auto& device = JUST(Device::New("cpu"));
  const auto& tensor = JUST(functional::Empty(*sizes, CHECK_JUST(DType::Get(dtype)), device));
  // Prevent the python object release until the callback is complete.
  Py_INCREF(object);
  auto handle = std::shared_ptr<PyObject>(PyObjectPtr(object));
  const auto& callback =
      std::make_shared<std::function<void(uint64_t)>>([handle](uint64_t of_blob_ptr) {
        auto* of_blob = reinterpret_cast<OfBlob*>(of_blob_ptr);
        CHECK_JUST(ParseArrayToBlob(handle.get(), of_blob->mut_blob()));
      });
  JUST(SpinCounter::SpinWait(1, [&](const std::shared_ptr<SpinCounter>& sc) -> Maybe<void> {
    return PhysicalRun([&](InstructionsBuilder* builder) -> Maybe<void> {
      return builder->SyncAccessBlobByCallback(JUST(tensor->AsMirroredTensor()), sc, callback,
                                               "mut");
    });
  }));
  return tensor;
}

Maybe<IndexItem> UnpackIndexItem(PyObject* object) {
  if (object == Py_Ellipsis) {
    return std::make_shared<IndexItem>(EllipsisIndex{});
  } else if (PySlice_Check(object)) {
    Py_ssize_t start, end, step;
    JUST(PySliceUnpack(object, &start, &end, &step));
    return std::make_shared<IndexItem>(start, end, step);
  } else if (PyLong_Check(object) && object != Py_False && object != Py_True) {
    return std::make_shared<IndexItem>(static_cast<int64_t>(PyLong_AsLongLong(object)));
  } else if (object == Py_False || object == Py_True) {
    return std::make_shared<IndexItem>(object == Py_True);
  } else if (object == Py_None) {
    return std::make_shared<IndexItem>(NoneIndex{});
  } else if (PyTensorCheck(object)) {
    auto obj = py::reinterpret_borrow<py::object>(object);
    return std::make_shared<IndexItem>(py::cast<std::shared_ptr<Tensor>>(obj));
  } else if (PySequence_Check(object)) {
    return std::make_shared<IndexItem>(JUST(ConvertToIndexingTensor(object)));
  }
  UNIMPLEMENTED_THEN_RETURN() << "Invalid index of " << Py_TYPE(object)->tp_name;
}

}  // namespace detail

}  // namespace functional
}  // namespace one
}  // namespace oneflow
