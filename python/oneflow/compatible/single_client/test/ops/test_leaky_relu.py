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
import tensorflow as tf
import test_global_storage
from test_util import GenArgList, type_name_to_flow_type

import oneflow.compatible.single_client.unittest
from oneflow.compatible import single_client as flow

gpus = tf.config.experimental.list_physical_devices("GPU")
for gpu in gpus:
    tf.config.experimental.set_memory_growth(gpu, True)


def compare_with_tensorflow(device_type, x_shape, data_type, alpha):
    assert device_type in ["gpu", "cpu"]
    flow.clear_default_session()
    func_config = flow.FunctionConfig()
    func_config.default_data_type(flow.float)

    @flow.global_function(type="train", function_config=func_config)
    def LeakyReluJob():
        with flow.scope.placement(device_type, "0:0"):
            x = flow.get_variable(
                "x",
                shape=x_shape,
                dtype=type_name_to_flow_type[data_type],
                initializer=flow.random_uniform_initializer(minval=-10, maxval=10),
                trainable=True,
            )
            loss = flow.nn.leaky_relu(x, alpha=alpha)
            flow.optimizer.SGD(
                flow.optimizer.PiecewiseConstantScheduler([], [0.0001]), momentum=0
            ).minimize(loss)
            flow.watch(x, test_global_storage.Setter("x"))
            flow.watch_diff(x, test_global_storage.Setter("x_diff"))
            flow.watch(loss, test_global_storage.Setter("loss"))
            flow.watch_diff(loss, test_global_storage.Setter("loss_diff"))
            return loss

    of_out = LeakyReluJob().get()
    with tf.GradientTape(persistent=True) as tape:
        x = tf.Variable(test_global_storage.Get("x"))
        tf_out = tf.nn.leaky_relu(x, alpha)
    loss_diff = test_global_storage.Get("loss_diff")
    tf_x_diff = tape.gradient(tf_out, x, loss_diff)
    assert np.allclose(of_out.numpy(), tf_out.numpy(), rtol=1e-05, atol=1e-05)
    assert np.allclose(
        test_global_storage.Get("x_diff"), tf_x_diff.numpy(), rtol=1e-05, atol=1e-05
    )


@flow.unittest.skip_unless_1n1d()
class TestLeakyRelu(flow.unittest.TestCase):
    @unittest.skipIf(os.getenv("ONEFLOW_TEST_CPU_ONLY"), "only test cpu cases")
    def test_leaky_relu(test_case):
        arg_dict = OrderedDict()
        arg_dict["device_type"] = ["gpu", "cpu"]
        arg_dict["x_shape"] = [(10, 10, 20, 30), (10, 20)]
        arg_dict["data_type"] = ["float32", "double"]
        arg_dict["alpha"] = [0.1, -0.2, 2]
        for arg in GenArgList(arg_dict):
            compare_with_tensorflow(*arg)


if __name__ == "__main__":
    unittest.main()
