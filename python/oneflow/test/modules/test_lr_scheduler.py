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

import math
import unittest

import oneflow as flow
import oneflow.unittest
from oneflow.nn.parameter import Parameter


@flow.unittest.skip_unless_1n1d()
class TestLrScheduler(flow.unittest.TestCase):
    base_lr = 1.0

    def test_cosine_decay_lr(test_case):
        optimizer = flow.optim.SGD(
            [{"params": [Parameter(flow.Tensor([1.0]))]}], lr=TestLrScheduler.base_lr
        )

        def cosine_decay_lr_step(base_lr, current_step, decay_steps, alpha):
            if current_step < decay_steps:
                cos_decay = 0.5 * (1 + math.cos(math.pi * current_step / decay_steps))
                decay_factor = (1 - alpha) * cos_decay + alpha
                return base_lr * decay_factor
            else:
                return base_lr * alpha

        alpha = 0.5
        decay_steps = 10
        cosine_decay_lr = flow.optim.lr_scheduler.CosineDecayLR(
            optimizer, decay_steps=decay_steps, alpha=alpha
        )
        for i in range(1, 21):
            cosine_decay_lr.step()
            new_lr = cosine_decay_lr_step(
                TestLrScheduler.base_lr, i, decay_steps, alpha
            )
            test_case.assertAlmostEqual(
                cosine_decay_lr.get_last_lr()[0], new_lr, places=4
            )

    def test_cosine_annealing_lr(test_case):
        optimizer = flow.optim.SGD(
            [{"params": [Parameter(flow.Tensor([1.0]))]}], lr=TestLrScheduler.base_lr
        )

        def cosine_annealing_lr_step(base_lr, current_step, last_lr, T_max, eta_min):
            if (current_step - 1 - T_max) % (2 * T_max) == 0:
                return (
                    last_lr
                    + (TestLrScheduler.base_lr - eta_min)
                    * (1 - math.cos(math.pi / T_max))
                    / 2
                )
            else:
                return (1 + math.cos(math.pi * current_step / T_max)) / (
                    1 + math.cos(math.pi * (current_step - 1) / T_max)
                ) * (last_lr - eta_min) + eta_min

        T_max = 20
        eta_min = 0.5
        cosine_annealing_lr = flow.optim.lr_scheduler.CosineAnnealingLR(
            optimizer, T_max=T_max, eta_min=eta_min
        )
        numpy_last_lr = TestLrScheduler.base_lr
        for i in range(1, 101):
            cosine_annealing_lr.step()
            numpy_last_lr = cosine_annealing_lr_step(
                TestLrScheduler.base_lr, i, numpy_last_lr, T_max, eta_min
            )
            test_case.assertAlmostEqual(
                cosine_annealing_lr.get_last_lr()[0], numpy_last_lr, places=4
            )

    def test_step_lr(test_case):
        optimizer = flow.optim.SGD(
            [{"params": [Parameter(flow.Tensor([1.0]))]}], lr=TestLrScheduler.base_lr
        )

        def step_lr_step(base_lr, current_step, step_size, gamma):
            return base_lr * gamma ** (current_step // step_size)

        gamma = 0.1
        step_size = 5
        step_lr = flow.optim.lr_scheduler.StepLR(
            optimizer, step_size=step_size, gamma=gamma
        )
        for i in range(1, 21):
            step_lr.step()
            new_lr = step_lr_step(TestLrScheduler.base_lr, i, step_size, gamma)
            test_case.assertAlmostEqual(step_lr.get_last_lr()[0], new_lr, places=5)

    def test_multistep_lr(test_case):
        optimizer = flow.optim.SGD(
            [{"params": [Parameter(flow.Tensor([1.0]))]}], lr=TestLrScheduler.base_lr
        )

        def multistep_lr_step(base_lr, current_step, milestones, gamma):
            count = 0
            for step in milestones:
                if current_step >= step:
                    count += 1
            return base_lr * gamma ** count

        gamma = 0.1
        milestones = [5, 11, 15]
        multistep_lr = flow.optim.lr_scheduler.MultiStepLR(
            optimizer, milestones=milestones, gamma=gamma
        )
        for i in range(1, 18):
            multistep_lr.step()
            new_lr = multistep_lr_step(TestLrScheduler.base_lr, i, milestones, gamma)
            test_case.assertAlmostEqual(multistep_lr.get_last_lr()[0], new_lr, places=5)

    def test_lambda_lr(test_case):
        optimizer = flow.optim.SGD(
            [
                {"params": [Parameter(flow.Tensor([1.0]))]},
                {"params": [Parameter(flow.Tensor([1.0]))]},
            ],
            lr=TestLrScheduler.base_lr,
        )
        lambdas = [lambda step: step // 30, lambda step: 0.95 * step]

        def lambda_lr_step(base_lrs, current_step):
            return [
                base_lr * lmbda(current_step)
                for (base_lr, lmbda) in zip(base_lrs, lambdas)
            ]

        lambda_lr = flow.optim.lr_scheduler.LambdaLR(optimizer, lr_lambda=lambdas)
        for i in range(1, 21):
            lambda_lr.step()
            new_lrs = lambda_lr_step(lambda_lr.base_lrs, i)
            for (lr1, lr2) in zip(lambda_lr.get_last_lr(), new_lrs):
                test_case.assertAlmostEqual(lr1, lr2, places=5)


if __name__ == "__main__":
    unittest.main()
