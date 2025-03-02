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
import oneflow as flow
from collections import OrderedDict
from automated_test_util import *
from test_util import GenArgList
import numpy as np
import unittest


def _test_randperm_with_generator(test_case, N, device, dtype):
    generator = flow.Generator()
    generator.manual_seed(0)
    y_1 = flow.randperm(N, device=device, dtype=dtype, generator=generator)
    generator = flow.Generator()
    generator.manual_seed(0)
    y_2 = flow.randperm(N, device=device, dtype=dtype, generator=generator)
    test_case.assertTrue(np.allclose(y_1.numpy(), y_2.numpy()))
    test_case.assertTrue(
        y_1.device == flow.device(device) and y_2.device == flow.device(device)
    )
    test_case.assertTrue(y_1.dtype == dtype and y_2.dtype == dtype)


def _test_randperm_backward(test_case, N, device, dtype):
    x = flow.randperm(N, device=device, dtype=dtype)
    x.requires_grad = True
    y = x.sum()
    y.backward()
    test_case.assertTrue(np.allclose(x.grad.numpy(), np.ones(N), 1e-05, 1e-05))


def _test_randperm_randomness(test_case, N, device, dtype):
    n = np.random.randint(100, 1000)
    x1 = flow.randperm(n, device=device)
    x2 = flow.randperm(n, device=device)
    test_case.assertFalse(np.all(x1.numpy() == x2.numpy()))


@flow.unittest.skip_unless_1n1d()
class Testrandperm(flow.unittest.TestCase):
    def test_randperm(test_case):
        arg_dict = OrderedDict()
        arg_dict["test_functions"] = [
            _test_randperm_with_generator,
            _test_randperm_backward,
            _test_randperm_randomness,
        ]
        arg_dict["N"] = [i for i in range(10, 100, 5)]
        arg_dict["device"] = ["cpu", "cuda"]
        arg_dict["dtype"] = [flow.int32, flow.int64, flow.float32, flow.float64]
        for arg in GenArgList(arg_dict):
            arg[0](test_case, *arg[1:])

    @autotest(auto_backward=False)
    def test_auto_1(test_case):
        device = random_device()
        y = torch.randperm(1, device=device)
        return y

    @autotest(n=5, auto_backward=False)
    def test_auto_0(test_case):
        device = random_device()
        y = torch.randperm(0, device=device)
        return y


if __name__ == "__main__":
    unittest.main()
