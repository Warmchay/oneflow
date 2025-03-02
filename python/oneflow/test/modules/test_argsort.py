"""
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
"""

import unittest
from collections import OrderedDict

import numpy as np
from test_util import GenArgList, type_name_to_flow_type

import oneflow as flow
import oneflow.unittest
from automated_test_util import *


def _test_argsort(test_case, data_shape, axis, descending, data_type, device):
    input = flow.Tensor(
        np.random.randn(*data_shape),
        dtype=type_name_to_flow_type[data_type],
        device=flow.device(device),
    )
    of_out = flow.argsort(input, dim=axis, descending=descending)
    np_input = -input.numpy() if descending else input.numpy()
    np_out = np.argsort(np_input, axis=axis)
    test_case.assertTrue(np.array_equal(of_out.numpy().flatten(), np_out.flatten()))


def _test_tensor_argsort(test_case, data_shape, axis, descending, data_type, device):
    input = flow.Tensor(
        np.random.randn(*data_shape),
        dtype=type_name_to_flow_type[data_type],
        device=flow.device(device),
    )
    of_out = input.argsort(dim=axis, descending=descending)
    np_input = -input.numpy() if descending else input.numpy()
    np_out = np.argsort(np_input, axis=axis)
    test_case.assertTrue(np.array_equal(of_out.numpy().shape, np_out.shape))
    test_case.assertTrue(np.array_equal(of_out.numpy().flatten(), np_out.flatten()))


@flow.unittest.skip_unless_1n1d()
class TestArgsort(flow.unittest.TestCase):
    def test_argsort(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_argsort, _test_tensor_argsort]
        arg_dict["data_shape"] = [(2, 6, 5, 4), (3, 4, 8)]
        arg_dict["axis"] = [-1, 0, 2]
        arg_dict["descending"] = [True, False]
        arg_dict["data_type"] = ["double", "float32", "int32"]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @autotest(auto_backward=False)
    def test_argsort_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=4).to(device)
        y = torch.argsort(
            x, dim=random(low=-4, high=4).to(int), descending=random_bool()
        )
        return y


if __name__ == "__main__":
    unittest.main()
