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
import os
import unittest
from collections import OrderedDict

import numpy as np
import oneflow as flow

import oneflow.unittest
from test_util import GenArgList


def _test_rand(test_case, device, shape, low, high):
    y1 = flow.randint(low, high, shape, device=flow.device(device))
    y2 = flow.randint(low, high, shape, device=flow.device(device))
    test_case.assertFalse(np.all(y1.numpy() == y2.numpy()))
    test_case.assertTrue(shape == y1.shape)


def _test_0d_rand(test_case, device, shape, low, high):
    y1 = flow.randint(low, high, shape, device=flow.device(device))
    y2 = flow.randint(low, high, shape, device=flow.device(device))
    test_case.assertTrue(
        np.allclose(y1.numpy(), y2.numpy(), atol=1e-4, rtol=1e-4)
    )  # 0d is [] and []
    test_case.assertTrue(shape == y1.shape)


def _test_different_dtype(test_case, device, shape, low, high):
    y1 = flow.randint(low, high, shape, dtype=flow.float32, device=flow.device(device))
    y2 = flow.randint(low, high, shape, dtype=flow.float64, device=flow.device(device))
    test_case.assertTrue(not np.allclose(y1.numpy(), y2.numpy(), atol=1e-4, rtol=1e-4))
    test_case.assertTrue(shape == y1.shape)


def _test_backward(test_case, device, shape, low, high):
    x = flow.randint(low, high, shape, device=flow.device(device), requires_grad=True)
    y = x.sum()
    y.backward()
    test_case.assertTrue(
        np.allclose(np.ones(shape), x.grad.numpy(), atol=1e-4, rtol=1e-4)
    )


def _test_with_generator(test_case, device, shape, low, high):
    gen = flow.Generator()
    gen.manual_seed(0)
    y1 = flow.randint(
        low, high, shape, dtype=flow.float32, device=flow.device(device), generator=gen
    )
    y1_np = y1.numpy()
    gen.manual_seed(0)
    y2 = flow.randint(
        low, high, shape, dtype=flow.float32, device=flow.device(device), generator=gen
    )
    test_case.assertTrue(np.allclose(y1_np, y2.numpy(), atol=1e-4, rtol=1e-4))


def _test_high(test_case, device, shape, low, high):
    y1 = flow.randint(low, high, shape, device=flow.device(device))
    y2 = flow.randint(low, high, shape, device=flow.device(device))
    test_case.assertFalse(np.all(y1.numpy() == y2.numpy()))
    test_case.assertTrue(shape == y1.shape)


def _test_0rank(test_case, device, shape, low, high):
    y1 = flow.randint(low, high, shape, device=flow.device(device))
    test_case.assertTrue(y1.shape == shape)


@flow.unittest.skip_unless_1n1d()
@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
class TestRandint(flow.unittest.TestCase):
    def test_consistent_naive(test_case):
        placement = flow.placement("cpu", {0: [0]})
        sbp = (flow.sbp.broadcast,)
        x = flow.randint(0, 16, (10, 1), placement=placement, sbp=sbp)
        test_case.assertEqual(x.sbp, sbp)
        test_case.assertEqual(x.placement, placement)

    def test_randint(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [
            _test_rand,
            _test_different_dtype,
            _test_backward,
            _test_with_generator,
        ]
        arg_dict["device"] = ["cpu", "cuda"]
        arg_dict["shape"] = [(2, 3), (2, 3, 4), (2, 3, 4, 5)]
        arg_dict["low"] = [i for i in range(10)]
        arg_dict["high"] = [10 + np.random.randint(10, 20) for i in range(10)]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    def test_0d_randint(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_0d_rand]
        arg_dict["device"] = ["cpu", "cuda"]
        arg_dict["shape"] = [(2, 0, 4), (2, 0, 2)]
        arg_dict["low"] = [i for i in range(10)]
        arg_dict["high"] = [10 + np.random.randint(1, 20) for i in range(10)]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    def test_high_randint(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_high]
        arg_dict["device"] = ["cpu", "cuda"]
        arg_dict["shape"] = [(2, 3, 4), (2, 5, 2)]
        arg_dict["low"] = [i for i in range(10)]
        arg_dict["high"] = [10 + np.random.randint(10, 20) for i in range(10)]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    def test_0rank_randint(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_0rank]
        arg_dict["device"] = ["cpu", "cuda"]
        arg_dict["shape"] = [()]
        arg_dict["low"] = [i for i in range(10)]
        arg_dict["high"] = [1000 + np.random.randint(1, 10) for i in range(10)]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])


if __name__ == "__main__":
    unittest.main()
