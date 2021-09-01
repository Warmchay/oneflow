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
import collections
import math
from typing import Callable, Dict, Iterator, List, Tuple, Union

import oneflow as flow
from oneflow.nn.optimizer.optimizer import Optimizer, ParamGroup
from oneflow.nn.parameter import Parameter


class Adagrad(Optimizer):
    """Implements Adagrad algorithm.
    """

    def __init__(
        self,
        params: Union[Iterator[Parameter], List[Dict]],
        lr: float = 0.001,
        lr_decay: float = 0.0,
        weight_decay: float = 0,
        initial_accumulator_value: float = 0.0,
        eps: float = 1e-10,
    ):
        assert lr >= 0.0, f"Invalid learning rate: {lr}"
        assert weight_decay >= 0.0, f"Invalid weight_decay value: {weight_decay}"
        assert (
            initial_accumulator_value >= 0.0
        ), f"Invalid initial_accumulator_value value: {initial_accumulator_value}"
        assert eps >= 0.0, f"Invalid epsilon value: {eps}"

        options = dict()
        options["lr"] = lr
        options["lr_decay"] = lr_decay
        options["weight_decay"] = weight_decay
        options["eps"] = eps
        super().__init__(params, options)

        for param_group in self.param_groups:
            for param in param_group.parameters:
                assert param.is_leaf, "parameters must be leaf tensor"
                self._state[param] = dict()
                self._state[param]["sum"] = flow.zeros_like(param).fill_(
                    initial_accumulator_value
                )

        self._op = (
            flow.builtin_op("adagrad_update")
            .Input("model")
            .Input("model_diff")
            .Input("sum")
            .Attr("l1", 0.0)
            .Attr("weight_decay", 0.0)
            .Build()
        )

    def step(self, closure: Callable = None):
        """Performs a single optimization step.

        Args:
            closure (callable, optional): A closure that reevaluates the model
                and returns the loss.
        """
        with flow.no_grad():
            loss = None
            if closure is not None:
                loss = closure()
            for param_group in self.param_groups:
                kwargs = {
                    "learning_rate_val": param_group["lr"],
                    "l2": param_group["weight_decay"],
                    "epsilon": param_group["eps"],
                    "lr_decay": param_group["lr_decay"],
                    "train_step_val": self._state["step"] + 1,
                }
                for param in param_group.parameters:
                    if param.grad is None:
                        continue
                    sum_tensor = self._state[param]["sum"]
                    self._op(param, param.grad, sum_tensor, **kwargs)

            self._state["step"] = self._state["step"] + 1
            return loss

    def generate_conf_for_graph(self, train_conf, vars_conf):
        new_opt_confs = []
        for param_group in self.param_groups:
            optimizer_conf = train_conf.mutable_optimizer_conf().Add()

            lr = (
                param_group["initial_lr"]
                if "initial_lr" in param_group
                else param_group["lr"]
            )
            l2 = param_group["weight_decay"]
            lr_decay = param_group["lr_decay"][0]
            epsilon = param_group["eps"]

            optimizer_conf.set_base_learning_rate(lr)
            optimizer_conf.mutable_adam_conf().set_lr_decay(lr_decay)
            optimizer_conf.mutable_adam_conf().set_epsilon(epsilon)

            self._generate_grad_clip_conf_for_optim_conf(param_group, optimizer_conf)

            for param in param_group.parameters:
                vars_conf[param].l2 = l2
                if param.requires_grad:
                    optimizer_conf.add_variable_op_names(vars_conf[param].name)

            new_opt_confs.append(optimizer_conf)
        return new_opt_confs
