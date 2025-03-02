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
from test_util import GenArgList

import oneflow as flow
import oneflow.unittest
from oneflow import tensor
import oneflow


def _test_local_tensor_str(test_case, device):
    # int dtype
    x = flow.tensor([[1, 2, 3], [4, 5, -6]], device=flow.device(device))
    tensor_str = str(x)
    test_case.assertTrue("3" in tensor_str)
    test_case.assertTrue("5" in tensor_str)
    test_case.assertTrue("-6" in tensor_str)
    test_case.assertTrue("2" in str(x[0][1]))
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # empty
    x = flow.tensor([], device=flow.device(device))
    tensor_str = str(x)
    test_case.assertTrue("[]" in tensor_str)
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # scientific representation int_mode(val == np.ceil(val))
    x = flow.tensor(
        [[1, 2, 3], [4, 5, 600000]], device=flow.device(device), dtype=flow.float64
    )
    tensor_str = str(x)
    test_case.assertTrue("6.0000e+05" in tensor_str)
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # int_mode
    x = flow.tensor(
        [[1.0, 2.0, 3.0], [4.0, 5, 60]], device=flow.device(device), dtype=flow.float64
    )
    tensor_str = str(x)
    test_case.assertTrue("4." in tensor_str)
    test_case.assertTrue("60." in tensor_str)
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # float dtype
    x = flow.tensor(
        [[1.3, 2.4, 3.5], [-4.6, 5, 60]], device=flow.device(device), dtype=flow.float64
    )
    tensor_str = str(x)
    test_case.assertTrue("3.5000" in tensor_str)
    test_case.assertTrue("-4.6000" in tensor_str)
    test_case.assertTrue("60.0000" in tensor_str)
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # scientific representation float dtype
    x = flow.tensor(
        [[1.3, 2.4, 3.5], [-4.6, 5, 60000000]],
        device=flow.device(device),
        dtype=flow.float64,
    )
    tensor_str = str(x)
    test_case.assertTrue("2.4000e+00" in tensor_str)
    test_case.assertTrue("3.5000e+00" in tensor_str)
    test_case.assertTrue("-4.6000e+00" in tensor_str)
    test_case.assertTrue("6.0000e+07" in tensor_str)
    test_case.assertTrue(np.allclose(eval(tensor_str).numpy(), x.numpy()))

    # summarized data float dtype
    x = flow.tensor(
        np.ones((100, 100, 100)), device=flow.device(device), dtype=flow.float64
    )
    tensor_str = str(x)
    test_case.assertTrue("1" in tensor_str)
    test_case.assertTrue("..." in tensor_str)


def _test_consistent_tensor_str(test_case, device):
    placement = flow.placement(device, {0: range(1)})
    # split consistent tensor
    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.split(0)])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)

    # broadcast consistent tensor
    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.broadcast])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)

    # partial_sum consistent tensor
    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.partial_sum])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)

    # summarized consistent tensor
    x = flow.ones((100, 100), placement=placement, sbp=[flow.sbp.split(0)])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)
    test_case.assertTrue("..." in tensor_str)

    # empty consistent tensor
    x = flow.ones((0, 10), placement=placement, sbp=[flow.sbp.split(0)])
    tensor_str = str(x)
    test_case.assertTrue("[]" in tensor_str)


def _test_consistent_tensor_str_2d(test_case, device):
    placement = flow.placement(device, {0: range(2)})
    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.split(0)])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)

    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.broadcast])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)
    # TODO: x[0][0].to("cuda") has bug
    # test_case.assertTrue("1." in str(x[0][0]))

    x = flow.ones((10, 10), placement=placement, sbp=[flow.sbp.partial_sum])
    tensor_str = str(x)
    test_case.assertTrue("2." in tensor_str)

    x = flow.ones((100, 100), placement=placement, sbp=[flow.sbp.split(0)])
    tensor_str = str(x)
    test_case.assertTrue("1." in tensor_str)
    test_case.assertTrue("..." in tensor_str)


class TestTensorStrModule(flow.unittest.TestCase):
    @flow.unittest.skip_unless_1n1d()
    def test_local_tensor_str_1n1d(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [
            _test_local_tensor_str,
        ]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    @flow.unittest.skip_unless_1n1d()
    def test_consistent_tensor_str_1n1d(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [
            _test_consistent_tensor_str,
        ]
        arg_dict["device"] = ["cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    @flow.unittest.skip_unless_1n2d()
    def test_tensor_str_1n2d(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [
            _test_consistent_tensor_str_2d,
        ]
        arg_dict["device"] = ["cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])


if __name__ == "__main__":
    unittest.main()
