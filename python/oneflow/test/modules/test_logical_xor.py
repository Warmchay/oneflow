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
from test_util import GenArgList

import oneflow as flow
from automated_test_util import *


def _test_logical_xor_int(test_case, shape, device):
    np_input = np.random.randint(-2, 4, size=shape)
    np_other = np.random.randint(-2, 4, size=shape)
    input = flow.Tensor(np_input, dtype=flow.float32, device=flow.device(device))
    other = flow.Tensor(np_other, dtype=flow.float32, device=flow.device(device))
    of_out = flow.logical_xor(input, other)
    np_out = np.logical_xor(np_input, np_other)
    test_case.assertTrue(np.array_equal(of_out.numpy(), np_out))


def _test_logical_xor_float(test_case, shape, device):
    np_input = np.random.uniform(low=-5, high=5, size=shape)
    np_other = np.random.uniform(low=-5, high=5, size=shape)
    input = flow.Tensor(np_input, dtype=flow.float32, device=flow.device(device))
    other = flow.Tensor(np_other, dtype=flow.float32, device=flow.device(device))
    of_out = flow.logical_xor(input, other)
    np_out = np.logical_xor(np_input, np_other)
    test_case.assertTrue(np.array_equal(of_out.numpy(), np_out))


def _test_tensor_logical_xor_int(test_case, shape, device):
    np_input = np.random.randint(-2, 4, size=shape)
    np_other = np.random.randint(-2, 4, size=shape)
    input = flow.Tensor(np_input, dtype=flow.float32, device=flow.device(device))
    other = flow.Tensor(np_other, dtype=flow.float32, device=flow.device(device))
    of_out = input.logical_xor(other)
    np_out = np.logical_xor(np_input, np_other)
    test_case.assertTrue(np.array_equal(of_out.numpy(), np_out))


def _test_tensor_logical_xor_float(test_case, shape, device):
    np_input = np.random.uniform(low=-5, high=5, size=shape)
    np_other = np.random.uniform(low=-5, high=5, size=shape)
    input = flow.Tensor(np_input, dtype=flow.float32, device=flow.device(device))
    other = flow.Tensor(np_other, dtype=flow.float32, device=flow.device(device))
    of_out = input.logical_xor(other)
    np_out = np.logical_xor(np_input, np_other)
    test_case.assertTrue(np.array_equal(of_out.numpy(), np_out))


@flow.unittest.skip_unless_1n1d()
class TestLogicalXorModule(flow.unittest.TestCase):
    def test_logical_xor(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [
            _test_logical_xor_int,
            _test_tensor_logical_xor_int,
            _test_logical_xor_float,
            _test_tensor_logical_xor_float,
        ]
        arg_dict["shape"] = [(2, 3), (2, 4, 5)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @autotest(n=10, auto_backward=False)
    def test_logical_xor_with_random_data(test_case):
        device = random_device()
        shape = random_tensor().value().shape
        x1 = random_pytorch_tensor(len(shape), *shape, requires_grad=False).to(device)
        x2 = random_pytorch_tensor(len(shape), *shape, requires_grad=False).to(device)
        y = torch.logical_xor(x1, x2)
        return y


if __name__ == "__main__":
    unittest.main()
