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

import oneflow as flow
import oneflow.unittest
from automated_test_util import *


@flow.unittest.skip_unless_1n1d()
class TestModule(flow.unittest.TestCase):
    @autotest()
    def test_flow_matmul_with_random_data(test_case):
        k = random(1, 6)
        x = random_pytorch_tensor(ndim=2, dim1=k)
        y = random_pytorch_tensor(ndim=2, dim0=k)
        z = torch.matmul(x, y)
        return z

    @autotest()
    def test_flow_tensor_matmul_with_random_data(test_case):
        k = random(1, 6)
        x = random_pytorch_tensor(ndim=2, dim1=k)
        y = random_pytorch_tensor(ndim=2, dim0=k)
        return x.matmul(y)


if __name__ == "__main__":
    unittest.main()
