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
class TestAvgPoolingModule(flow.unittest.TestCase):
    @autotest(n=100)
    def test_avgpool1d_with_random_data(test_case):
        m = torch.nn.AvgPool1d(
            kernel_size=random(4, 6),
            stride=random(1, 3) | nothing(),
            padding=random(1, 3) | nothing(),
            ceil_mode=random(),
            count_include_pad=random(),
        )
        m.train(random())
        device = random_device()
        m.to(device)
        x = random_pytorch_tensor(ndim=3, dim2=random(20, 22)).to(device)
        y = m(x)
        return y

    @autotest(n=100)
    def test_avgpool2d_with_random_data(test_case):
        m = torch.nn.AvgPool2d(
            kernel_size=random(4, 6),
            stride=random(1, 3) | nothing(),
            padding=random(1, 3) | nothing(),
            ceil_mode=random(),
            count_include_pad=random(),
            divisor_override=random().to(int),
        )
        m.train(random())
        device = random_device()
        m.to(device)
        x = random_pytorch_tensor(ndim=4, dim2=random(20, 22), dim3=random(20, 22)).to(
            device
        )
        y = m(x)
        return y

    @autotest(n=100)
    def test_avgpool3d_with_random_data(test_case):
        m = torch.nn.AvgPool3d(
            kernel_size=random(4, 6),
            stride=random(1, 3) | nothing(),
            padding=random(1, 3) | nothing(),
            ceil_mode=random(),
            count_include_pad=random(),
            divisor_override=random().to(int),
        )
        m.train(random())
        device = random_device()
        m.to(device)
        x = random_pytorch_tensor(
            ndim=5, dim2=random(20, 22), dim3=random(20, 22), dim4=random(20, 22)
        ).to(device)
        y = m(x)
        return y


if __name__ == "__main__":
    unittest.main()
