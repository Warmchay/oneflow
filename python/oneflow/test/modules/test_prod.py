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
from automated_test_util import *
import oneflow as flow
import oneflow.unittest


@flow.unittest.skip_unless_1n1d()
class TestReduceProd(flow.unittest.TestCase):
    @autotest()
    def test_reduce_prod_without_dim(test_case):
        device = random_device()
        ndim = random(1, 5).to(int)
        x = random_pytorch_tensor(ndim=ndim).to(device)
        y = torch.prod(x)

        return y

    @autotest()
    def test_reduce_prod_with_dim(test_case):
        device = random_device()
        ndim = random(1, 5).to(int)
        x = random_pytorch_tensor(ndim=ndim).to(device)
        dim = random(0, ndim).to(int)
        y = torch.prod(x, dim)
        y = torch.exp(y)

        return y


if __name__ == "__main__":
    unittest.main()
