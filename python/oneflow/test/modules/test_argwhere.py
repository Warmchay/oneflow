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
import oneflow.unittest
from automated_test_util import *


def _test_argwhere(test_case, shape, device):
    np_input = np.random.randn(*shape)
    input = flow.Tensor(np_input, device=flow.device(device))
    of_out = flow.argwhere(input)
    np_out = np.argwhere(np_input)
    test_case.assertTrue(np.allclose(of_out.numpy(), np_out, 0.0001, 0.0001))
    test_case.assertTrue(np.array_equal(of_out.numpy().shape, np_out.shape))


@flow.unittest.skip_unless_1n1d()
class TestArgwhere(flow.unittest.TestCase):
    def test_argwhere(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_fun"] = [_test_argwhere]
        arg_dict["shape"] = [(2, 3), (2, 3, 4), (2, 4, 5, 6), (2, 3, 0, 4)]
        arg_dict["device"] = ["cpu", "cuda"]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @unittest.skip("pytorch do not have argwhere fn/module yet!")
    @autotest(n=5, rtol=1e-5, atol=1e-5)
    def test_argwhere_with_random_data(test_case):
        device = random_device()
        x = random_pytorch_tensor(ndim=random(2, 5).to(int)).to(device)
        y = torch.argwhere(x)
        return y


if __name__ == "__main__":
    unittest.main()
