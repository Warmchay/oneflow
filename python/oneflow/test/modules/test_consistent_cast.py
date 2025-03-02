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
import oneflow as flow
import os

import oneflow.unittest
from test_util import GenArgList


@flow.unittest.skip_unless_1n4d()
class TestConsistentCastModule_1n4d(flow.unittest.TestCase):
    def test_to_consistent_flatten_hierarchy(test_case):
        x = flow.ones((16, 16), dtype=flow.int32)
        sbp = (flow.sbp.partial_sum,)
        y = x.to_consistent(
            placement=flow.placement("cpu", {0: range(4)}, hierarchy=(2, 2)),
            sbp=(flow.sbp.partial_sum, flow.sbp.partial_sum),
        )
        placement = flow.placement("cpu", {0: range(4)})
        y = y.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))

    def test_to_consistent_flatten_hierarchy_cpu_to_gpu(test_case):
        x = flow.ones((16, 16), dtype=flow.int32)
        sbp = (flow.sbp.partial_sum,)
        y = x.to_consistent(
            placement=flow.placement("cpu", {0: range(4)}, hierarchy=(2, 2)),
            sbp=(flow.sbp.partial_sum, flow.sbp.partial_sum),
        )
        placement = flow.placement("cuda", {0: range(4)})
        y = y.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))

    def test_to_consistent_flatten_hierarchy_gpu_to_cpu(test_case):
        x = flow.ones((16, 16), dtype=flow.int32)
        sbp = (flow.sbp.partial_sum,)
        y = x.to_consistent(
            placement=flow.placement("cuda", {0: range(4)}, hierarchy=(2, 2)),
            sbp=(flow.sbp.partial_sum, flow.sbp.partial_sum),
        )
        placement = flow.placement("cpu", {0: range(4)})
        y = y.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))

    def test_to_consistent_broadcast_shape_dtype(test_case):
        if int(os.getenv("RANK")) < 2:
            x = flow.ones((16, 16), dtype=flow.int32)
        else:
            x = flow.zeros((1,), dtype=flow.float)
        placement = flow.placement("cpu", {0: range(2)})
        sbp = (flow.sbp.split(0),)
        y = x.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)

    def test_to_consistent_loop_broadcast_shape_dtype(test_case):
        if int(os.getenv("RANK")) < 2:
            x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
            a = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        else:
            x = flow.zeros((1,), dtype=flow.float)
            a = flow.zeros((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        sbp = (flow.sbp.split(0),)
        for i in range(1000):
            if i % 100 == 0:
                print(i)
            y = x.to_consistent(placement=placement, sbp=sbp)
            b = a.to_consistent(placement=placement, sbp=flow.sbp.broadcast)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)


@flow.unittest.skip_unless_1n2d()
class TestConsistentCastModule_1n2d(flow.unittest.TestCase):
    def test_to_consistent_broadcast_shape_dtype(test_case):
        if os.getenv("RANK") == "0":
            x = flow.ones((16, 16), dtype=flow.int32)
        else:
            x = flow.zeros((1,), dtype=flow.float)
        placement = flow.placement("cpu", {0: [0]})
        sbp = (flow.sbp.broadcast,)
        y = x.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)

    def test_local_to_consistent_broadcast_data(test_case):
        if int(os.getenv("RANK")) == 0:
            x = flow.ones((16, 16), dtype=flow.int32)
        else:
            x = flow.zeros((16, 16), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        sbp = (flow.sbp.broadcast,)
        y = x.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((16, 16), dtype=np.int32))
        )

    def test_cuda_consistent_to_consistent_cpu_s2b(test_case):
        x = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.split(0))
        sbp = (flow.sbp.broadcast,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((32, 16), dtype=np.int32))
        )

    def test_cuda_consistent_to_consistent_s2b(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.split(0))
        sbp = (flow.sbp.broadcast,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((32, 16), dtype=np.int32))
        )

    def test_cuda_consistent_to_consistent_cpu_s2p(test_case):
        x = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.split(0))
        sbp = (flow.sbp.partial_sum,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        if int(os.getenv("RANK")) == 0:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.ones((32, 16), dtype=np.int32))
            )
        else:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.zeros((32, 16), dtype=np.int32))
            )

    def test_cuda_consistent_to_consistent_s2p(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.split(0))
        sbp = (flow.sbp.partial_sum,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (32, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        if int(os.getenv("RANK")) == 0:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.ones((32, 16), dtype=np.int32))
            )
        else:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.zeros((32, 16), dtype=np.int32))
            )

    def test_cuda_consistent_to_consistent_b2p(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.broadcast)
        sbp = (flow.sbp.partial_sum,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        if int(os.getenv("RANK")) == 0:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.ones((16, 16), dtype=np.int32))
            )
        else:
            test_case.assertTrue(
                np.array_equal(z.numpy(), np.zeros((16, 16), dtype=np.int32))
            )

    def test_cuda_consistent_to_consistent_b2s(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.broadcast)
        sbp = (flow.sbp.split(0),)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((8, 16), dtype=np.int32))
        )

    def test_cuda_consistent_to_consistent_cpu_p2s(test_case):
        x = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.partial_sum)
        sbp = (flow.sbp.split(0),)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((8, 16), dtype=np.int32) * 2)
        )

    def test_cuda_consistent_to_consistent_p2s(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.partial_sum)
        sbp = (flow.sbp.split(0),)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((8, 16), dtype=np.int32) * 2)
        )

    def test_cuda_consistent_to_consistent_cuda_h2d(test_case):
        x = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        cuda_placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.partial_sum)
        y = y.to_consistent(placement=cuda_placement, sbp=flow.sbp.partial_sum)
        test_case.assertEqual(y.sbp, (flow.sbp.partial_sum,))
        test_case.assertEqual(y.placement, cuda_placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((16, 16), dtype=np.int32))
        )

    def test_cuda_consistent_to_consistent_cpu_p2b(test_case):
        x = flow.ones((16, 16), device=flow.device("cpu"), dtype=flow.int32)
        placement = flow.placement("cpu", {0: range(2)})
        cuda_placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.partial_sum)
        import time

        y = y.to_consistent(placement=cuda_placement, sbp=flow.sbp.partial_sum)
        sbp = (flow.sbp.broadcast,)
        y = y.to_consistent(placement=cuda_placement, sbp=sbp)
        y = y.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((16, 16), dtype=np.int32) * 2)
        )

    def test_cuda_consistent_to_consistent_p2b(test_case):
        x = flow.ones((16, 16), device=flow.device("cuda"), dtype=flow.int32)
        placement = flow.placement("cuda", {0: range(2)})
        y = x.to_consistent(placement=placement, sbp=flow.sbp.partial_sum)
        sbp = (flow.sbp.broadcast,)
        y = y.to_consistent(sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))
        test_case.assertEqual(y.dtype, flow.int32)
        z = y.to_local()
        test_case.assertTrue(
            np.array_equal(z.numpy(), np.ones((16, 16), dtype=np.int32) * 2)
        )


@flow.unittest.skip_unless_1n1d()
class TestConsistentCastModule_1n1d(flow.unittest.TestCase):
    def test_to_consistent(test_case):
        x = flow.ones((16, 16))
        placement = flow.placement("cpu", {0: [0]})
        sbp = (flow.sbp.broadcast,)
        y = x.to_consistent(placement=placement, sbp=sbp)
        test_case.assertEqual(y.sbp, sbp)
        test_case.assertEqual(y.placement, placement)
        test_case.assertEqual(tuple(y.shape), (16, 16))


class TestConsistentCast(flow.unittest.TestCase):
    @flow.unittest.skip_unless_1n4d()
    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    def test_cpu_local_tensor_to_gpu_placement(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array([4, 6, 7, 8], dtype=np.float32)
        else:
            np_arr = np.array([0, 0, 0, 0], dtype=np.float32)
        tensor = flow.Tensor(np_arr, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(4)})
        device = flow.device("cuda")
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.broadcast)
        test_case.assertTrue(consistent_tensor.to_local().device == device)
        test_case.assertTrue(consistent_tensor.placement == placement)
        test_case.assertTrue(
            np.array_equal(
                consistent_tensor.to_local().numpy(),
                np.array([4, 6, 7, 8], dtype=np.float32),
            )
        )

    @flow.unittest.skip_unless_1n4d()
    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    def test_local_to_consistent_with_wrong_device(test_case):
        np_arr = np.array([4, 6], dtype=np.float32)
        tensor = flow.Tensor(
            np_arr,
            device=flow.device("cuda:%d" % ((flow.env.get_rank() + 1) % 4)),
            dtype=flow.float32,
        )
        placement = flow.placement("cuda", {0: range(4)})
        device = flow.device("cuda")
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.broadcast)
        local_tensor = consistent_tensor.to_local()
        test_case.assertTrue(local_tensor.device == device)
        test_case.assertTrue(consistent_tensor.placement == placement)


class TestConsistentCast_S2S(flow.unittest.TestCase):
    @flow.unittest.skip_unless_1n2d()
    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    def test_consistent_to_consistent_s0ts1(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        split0_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        split1_tensor = split0_tensor.to_consistent(placement, flow.sbp.split(1))
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    split1_tensor.to_local().numpy(),
                    np.array(
                        [
                            [4.0, 6.0],
                            [6.0, 2.0],
                            [3.0, 7.0],
                            [6.0, 8.0],
                            [2.0, 10.0],
                            [3.0, 9.0],
                            [4.0, 6.0],
                            [6.0, 8.0],
                        ],
                        dtype=np.float32,
                    ),
                )
            )
        else:
            test_case.assertTrue(
                np.array_equal(
                    split1_tensor.to_local().numpy(),
                    np.array(
                        [
                            [5.0, 20.0],
                            [5.0, 7.0],
                            [5.0, 4.0],
                            [9.0, 4.0],
                            [10.0, 7.0],
                            [10.0, 5.0],
                            [6.0, 9.0],
                            [6.0, 4.0],
                        ],
                        dtype=np.float32,
                    ),
                )
            )

    @flow.unittest.skip_unless_1n2d()
    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    def test_consistent_to_consistent_s1ts0(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        split_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        split1_tensor = split_tensor.to_consistent(placement, flow.sbp.split(1))
        split0_tensor = split1_tensor.to_consistent(placement, flow.sbp.split(0))
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    split0_tensor.to_local().numpy(),
                    np.array(
                        [
                            [4.0, 6.0, 5.0, 20.0],
                            [6.0, 2.0, 5.0, 7.0],
                            [3.0, 7.0, 5.0, 4.0],
                            [6.0, 8.0, 9.0, 4.0],
                        ],
                        dtype=np.float32,
                    ),
                )
            )
        else:
            test_case.assertTrue(
                np.array_equal(
                    split0_tensor.to_local().numpy(),
                    np.array(
                        [
                            [2.0, 10.0, 10.0, 7.0],
                            [3.0, 9.0, 10.0, 5.0],
                            [4.0, 6.0, 6.0, 9.0],
                            [6.0, 8.0, 6.0, 4.0],
                        ],
                        dtype=np.float32,
                    ),
                )
            )


@flow.unittest.skip_unless_1n4d()
@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
class TestConsistentCast_XToB(flow.unittest.TestCase):
    def test_consistent_to_consistent_btb_gpu_to_gpu(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 8, 9, 0], [3, 7, 5, 0], [6, 8, 9, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 1:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 2:
            np_arr = np.array(
                [[9, 6, 5, 8], [4, 9, 7, 0], [2, 5, 7, 9], [6, 8, 10, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 3:
            np_arr = np.array(
                [[9, 4, 5, 8], [7, 2, 9, 5], [6, 3, 9, 2], [3, 7, 5, 8]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.broadcast)
        new_placement = flow.placement("cuda", {0: range(3)})
        broadcast_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(broadcast_tensor.placement, new_placement)
        if flow.env.get_rank() != 3:
            test_case.assertTrue(
                np.array_equal(
                    broadcast_tensor.to_local().numpy(),
                    np.array(
                        [[4, 6, 5, 20], [6, 8, 9, 0], [3, 7, 5, 0], [6, 8, 9, 0]],
                        dtype=np.float32,
                    ),
                )
            )

    def test_consistent_to_consistent_stb_gpu_to_gpu(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 8, 9, 0], [3, 7, 5, 0], [6, 8, 9, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 1:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 2:
            np_arr = np.array(
                [[9, 6, 5, 8], [4, 9, 7, 0], [2, 5, 7, 9], [6, 8, 10, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 3:
            np_arr = np.array(
                [[9, 4, 5, 8], [7, 2, 9, 5], [6, 3, 9, 2], [3, 7, 5, 8]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(3)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        new_placement = flow.placement("cuda", {0: range(4)})
        broadcast_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(broadcast_tensor.placement, new_placement)
        test_case.assertTrue(
            np.array_equal(
                broadcast_tensor.to_local().numpy(),
                np.array(
                    [
                        [4, 6, 5, 20],
                        [6, 8, 9, 0],
                        [3, 7, 5, 0],
                        [6, 8, 9, 0],
                        [2, 10, 10, 7],
                        [3, 9, 10, 5],
                        [4, 6, 6, 9],
                        [6, 8, 6, 4],
                        [9, 6, 5, 8],
                        [4, 9, 7, 0],
                        [2, 5, 7, 9],
                        [6, 8, 10, 0],
                    ],
                    dtype=np.float32,
                ),
            )
        )

    def test_consistent_to_consistent_ptb_gpu_to_gpu(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 8, 9, 0], [3, 7, 5, 0], [6, 8, 9, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 1:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 2:
            np_arr = np.array(
                [[9, 6, 5, 8], [4, 9, 7, 0], [2, 5, 7, 9], [6, 8, 10, 0]],
                dtype=np.float32,
            )
        elif flow.env.get_rank() == 3:
            np_arr = np.array(
                [[9, 4, 5, 8], [7, 2, 9, 5], [6, 3, 9, 2], [3, 7, 5, 8]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(3)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.partial_sum)
        new_placement = flow.placement("cuda", {0: range(4)})
        broadcast_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(broadcast_tensor.placement, new_placement)
        test_case.assertTrue(
            np.array_equal(
                broadcast_tensor.to_local().numpy(),
                np.array(
                    [
                        [15, 22, 20, 35],
                        [13, 26, 26, 5],
                        [9, 18, 18, 18],
                        [18, 24, 25, 4],
                    ],
                    dtype=np.float32,
                ),
            )
        )


@flow.unittest.skip_unless_1n4d()
@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
class TestConsistentCast_1ToN(flow.unittest.TestCase):
    def test_consistent_to_consistent_1tb(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(1)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        new_placement = flow.placement("cuda", {0: range(2)})
        broadcast_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(broadcast_tensor.placement, new_placement)
        if flow.env.get_rank() < 2:
            test_case.assertTrue(
                np.array_equal(
                    broadcast_tensor.to_local().numpy(),
                    np.array(
                        [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                        dtype=np.float32,
                    ),
                )
            )

    def test_consistent_to_consistent_1tp(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(1)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        new_placement = flow.placement("cuda", {0: range(2)})
        partial_sum_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.partial_sum
        )
        test_case.assertTrue(partial_sum_tensor.placement, new_placement)
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    partial_sum_tensor.to_local().numpy(),
                    np.array(
                        [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                        dtype=np.float32,
                    ),
                )
            )
        elif flow.env.get_rank() == 1:
            test_case.assertTrue(
                np.array_equal(
                    partial_sum_tensor.to_local().numpy(),
                    np.array(
                        [[0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]],
                        dtype=np.float32,
                    ),
                )
            )

    def test_consistent_to_consistent_1ts(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(1)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        new_placement = flow.placement("cuda", {0: range(4)})
        split_tensor = consistent_tensor.to_consistent(new_placement, flow.sbp.split(0))
        test_case.assertTrue(split_tensor.placement, new_placement)
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    split_tensor.to_local().numpy(),
                    np.array([[4, 6, 5, 20]], dtype=np.float32,),
                )
            )
        elif flow.env.get_rank() == 1:
            test_case.assertTrue(
                np.array_equal(
                    split_tensor.to_local().numpy(),
                    np.array([[6, 2, 5, 7]], dtype=np.float32,),
                )
            )
        elif flow.env.get_rank() == 2:
            test_case.assertTrue(
                np.array_equal(
                    split_tensor.to_local().numpy(),
                    np.array([[3, 7, 5, 4]], dtype=np.float32,),
                )
            )
        elif flow.env.get_rank() == 3:
            test_case.assertTrue(
                np.array_equal(
                    split_tensor.to_local().numpy(),
                    np.array([[6, 8, 9, 4]], dtype=np.float32,),
                )
            )


@flow.unittest.skip_unless_1n4d()
@unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
class TestConsistentCast_NTo1(flow.unittest.TestCase):
    def test_consistent_to_consistent_bt1(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.broadcast)
        new_placement = flow.placement("cuda", {0: range(1)})
        broadcast_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(broadcast_tensor.placement, new_placement)
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    broadcast_tensor.to_local().numpy(),
                    np.array(
                        [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                        dtype=np.float32,
                    ),
                )
            )

    def test_consistent_to_consistent_st1(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.split(0))
        new_placement = flow.placement("cuda", {0: range(1)})
        partial_sum_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(partial_sum_tensor.placement, new_placement)
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    partial_sum_tensor.to_local().numpy(),
                    np.array(
                        [
                            [4, 6, 5, 20],
                            [6, 2, 5, 7],
                            [3, 7, 5, 4],
                            [6, 8, 9, 4],
                            [2, 10, 10, 7],
                            [3, 9, 10, 5],
                            [4, 6, 6, 9],
                            [6, 8, 6, 4],
                        ],
                        dtype=np.float32,
                    ),
                )
            )

    def test_consistent_to_consistent_pt1(test_case):
        if flow.env.get_rank() == 0:
            np_arr = np.array(
                [[4, 6, 5, 20], [6, 2, 5, 7], [3, 7, 5, 4], [6, 8, 9, 4]],
                dtype=np.float32,
            )
        else:
            np_arr = np.array(
                [[2, 10, 10, 7], [3, 9, 10, 5], [4, 6, 6, 9], [6, 8, 6, 4]],
                dtype=np.float32,
            )
        device = flow.device("cuda")
        tensor = flow.Tensor(np_arr, device=device, dtype=flow.float32)
        placement = flow.placement("cuda", {0: range(2)})
        consistent_tensor = tensor.to_consistent(placement, flow.sbp.partial_sum)
        new_placement = flow.placement("cuda", {0: range(1)})
        partial_sum_tensor = consistent_tensor.to_consistent(
            new_placement, flow.sbp.broadcast
        )
        test_case.assertTrue(partial_sum_tensor.placement, new_placement)
        if flow.env.get_rank() == 0:
            test_case.assertTrue(
                np.array_equal(
                    partial_sum_tensor.to_local().numpy(),
                    np.array(
                        [
                            [6, 16, 15, 27],
                            [9, 11, 15, 12],
                            [7, 13, 11, 13],
                            [12, 16, 15, 8],
                        ],
                        dtype=np.float32,
                    ),
                )
            )


if __name__ == "__main__":
    unittest.main()
