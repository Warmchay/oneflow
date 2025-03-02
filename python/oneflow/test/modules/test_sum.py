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
from automated_test_util import *
from test_util import GenArgList

import oneflow as flow
import oneflow.unittest


def _test_sum_impl(test_case, device):
    input = flow.Tensor(
        np.random.randn(2, 3), dtype=flow.float32, device=flow.device(device)
    )
    of_out = flow.sum(input, dim=0)
    np_out = np.sum(input.numpy(), axis=0)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-05, 1e-05))
    input = flow.Tensor(
        np.random.randn(2, 3), dtype=flow.float32, device=flow.device(device)
    )
    of_out = flow.sum(input, dim=0)
    np_out = np.sum(input.numpy(), axis=0)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-05, 1e-05))
    input = flow.Tensor(
        np.random.randn(2, 3), dtype=flow.float32, device=flow.device(device)
    )
    of_out = flow.sum(input, dim=1)
    of_out2 = input.sum(dim=1)
    np_out = np.sum(input.numpy(), axis=1)
    test_case.assertTrue(np.allclose(of_out2.numpy(), of_out.numpy(), 1e-05, 1e-05))
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-05, 1e-05))
    input = flow.Tensor(
        np.random.randn(4, 5, 6),
        dtype=flow.float32,
        device=flow.device(device),
        requires_grad=True,
    )
    of_out = flow.sum(input, dim=(2, 1))
    np_out = np.sum(input.numpy(), axis=(2, 1))
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 1e-05, 1e-05))
    of_out = of_out.sum()
    of_out.backward()
    np_grad = np.ones((4, 5, 6))
    test_case.assertTrue(np.allclose(input.grad.numpy(), np_grad, 1e-05, 1e-05))


@flow.unittest.skip_unless_1n1d()
class TestSumModule(flow.unittest.TestCase):
    def test_sum(test_case):
        arg_dict = OrderedDict()
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            _test_sum_impl(test_case, *arg)

    @autotest()
    def test_sum_against_pytorch(test_case):
        device = random_device()
        x = random_pytorch_tensor(4, random(0, 5), 2).to(device)
        y = torch.sum(x)
        return y

    @autotest(auto_backward=False)
    def test_sum_with_0shape_tensor(test_case):
        device = random_device()
        x = random_pytorch_tensor(4, 4, 3, 0, 2).to(device)
        y = torch.sum(x, dim=np.random.randint(0, 3))
        return y


if __name__ == "__main__":
    unittest.main()
