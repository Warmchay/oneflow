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


class Adam(Optimizer):
    """Implements Adam algorithm.

    It has been proposed in `Adam: A Method for Stochastic Optimization`_.
    The implementation of the L2 penalty follows changes proposed in
    `Decoupled Weight Decay Regularization`_.

    This algorithm can adjust the learning rate of each parameter dynamically according to the 1st-moment estimates and the 2nd-moment estimates of gradient.

    the equation of parameters updating is:

    .. math::

        & V_t = \\beta_1*V_{t-1} + (1-\\beta_1)*grad

        & S_t = \\beta_2*S_{t-1} + (1-\\beta_2)*{grad} \\odot {grad}

        & \\hat{g} = learning\\_rate*\\frac{{V_t}}{\\sqrt{{S_t}}+\\epsilon}

        & param_{new} = param_{old} - \\hat{g}

    Args:
        params (iterable): iterable of parameters to optimize or dicts defining
            parameter groups
        lr (float, optional): learning rate (default: 1e-3)
        betas (Tuple[float, float], optional): coefficients used for computing
            running averages of gradient and its square (default: (0.9, 0.999))
        eps (float, optional): term added to the denominator to improve
            numerical stability (default: 1e-8)
        weight_decay (float, optional): weight decay (L2 penalty) (default: 0)
        amsgrad (bool, optional): whether to use the AMSGrad variant of this algorithm. (default: False) 
        do_bias_correction (bool, optional): Whether do bias correction (default: True)

    .. _Adam\\: A Method for Stochastic Optimization:
        https://arxiv.org/abs/1412.6980
    .. _Decoupled Weight Decay Regularization:
        https://arxiv.org/abs/1711.05101

    """

    def __init__(
        self,
        parameters: Union[Iterator[Parameter], List[Dict]],
        lr: float = 0.001,
        betas: Tuple[float, float] = (0.9, 0.999),
        eps: float = 1e-08,
        weight_decay: float = 0,
        amsgrad: bool = False,
        do_bias_correction=True,
    ):
        assert lr >= 0.0, f"Invalid learning rate: {lr}"
        assert eps >= 0.0, f"Invalid epsilon value: {eps}"
        assert (
            betas[0] >= 0.0 and betas[0] < 1.0
        ), f"Invalid beta parameter at index 0: {betas[0]}"
        assert (
            betas[1] >= 0.0 and betas[1] < 1.0
        ), f"Invalid beta parameter at index 1: {betas[1]}"
        assert weight_decay >= 0.0, f"Invalid weight_decay value: {weight_decay}"
        options = dict()
        options["lr"] = lr
        options["eps"] = eps
        options["betas"] = betas
        options["weight_decay"] = weight_decay
        options["amsgrad"] = amsgrad
        options["do_bias_correction"] = do_bias_correction
        super().__init__(parameters, options)

        for param_group in self.param_groups:
            for param in param_group.parameters:
                assert param.is_leaf, "parameters must be leaf tensor"
                self._state[param] = dict()
                self._state[param]["exp_avg"] = flow.zeros_like(param)
                self._state[param]["exp_avg_sq"] = flow.zeros_like(param)
                self._state[param]["max_exp_avg_sq"] = flow.zeros_like(param)

        self._state["bias_correction1"] = flow.ones_like(param)
        self._state["bias_correction2"] = flow.ones_like(param)

        if options["do_bias_correction"]:
            self._op = (
                flow.builtin_op("adam_update")
                .Input("model")
                .Input("model_diff")
                .Input("bias_correction1")
                .Input("bias_correction2")
                .Input("m")
                .Input("v")
                .Input("max_v")
                .Attr("l1", 0.0)
                .Attr("weight_decay", 0.0)
                .Build()
            )
        else:
            self._op = (
                flow.builtin_op("adam_update")
                .Input("model")
                .Input("model_diff")
                .Input("m")
                .Input("v")
                .Input("max_v")
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
                    "beta1": param_group["betas"][0],
                    "beta2": param_group["betas"][1],
                    "epsilon": param_group["eps"],
                    "do_bias_correction": param_group["do_bias_correction"],
                    "amsgrad": param_group["amsgrad"],
                }
                if param_group["do_bias_correction"]:
                    self._state["bias_correction1"] = self._state[
                        "bias_correction1"
                    ].fill_(
                        (1 - math.pow(param_group["betas"][0], self._state["step"] + 1))
                    )
                    self._state["bias_correction2"] = self._state[
                        "bias_correction2"
                    ].fill_(
                        (1 - math.pow(param_group["betas"][1], self._state["step"] + 1))
                    )

                for param in param_group.parameters:
                    if param.grad is None:
                        continue
                    m_tensor = self._state[param]["exp_avg"]
                    v_tensor = self._state[param]["exp_avg_sq"]
                    max_v_tensor = self._state[param]["max_exp_avg_sq"]
                    if param_group["do_bias_correction"]:
                        bias_correction_tensor_a = self._state["bias_correction1"]
                        bias_correction_tensor_b = self._state["bias_correction2"]
                        # when set bias_correction tensor a as 1.0, it equal to no do bias correction.
                        self._op(
                            param,
                            param.grad,
                            bias_correction_tensor_a,
                            bias_correction_tensor_b,
                            m_tensor,
                            v_tensor,
                            max_v_tensor,
                            **kwargs,
                        )
                    else:
                        self._op(
                            param,
                            param.grad,
                            m_tensor,
                            v_tensor,
                            max_v_tensor,
                            **kwargs,
                        )

            self._state["step"] += 1

            return loss

    def generate_conf_for_graph(self, train_conf, vars_conf):
        new_opt_confs = []
        for param_group in self.param_groups:
            optimizer_conf = train_conf.mutable_optimizer_conf().Add()

            lr = param_group["lr"]
            l2 = param_group["weight_decay"]
            beta1 = param_group["betas"][0]
            beta2 = param_group["betas"][1]

            epsilon = param_group["eps"]
            do_bias_correction = param_group["do_bias_correction"]
            amsgrad = param_group["amsgrad"]
            optimizer_conf.set_base_learning_rate(lr)

            optimizer_conf.mutable_adam_conf().set_beta1(beta1)
            optimizer_conf.mutable_adam_conf().set_beta2(beta2)
            optimizer_conf.mutable_adam_conf().set_epsilon(epsilon)
            optimizer_conf.mutable_adam_conf().set_do_bias_correction(
                do_bias_correction
            )
            optimizer_conf.mutable_adam_conf().set_amsgrad(amsgrad)

            self._generate_grad_clip_conf_for_optim_conf(param_group, optimizer_conf)

            for param in param_group.parameters:
                vars_conf[param].l2 = l2
                if param.requires_grad:
                    optimizer_conf.add_variable_op_names(vars_conf[param].name)

            new_opt_confs.append(optimizer_conf)
        return new_opt_confs
