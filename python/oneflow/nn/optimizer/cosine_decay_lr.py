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

from .lr_scheduler import LrScheduler


class CosineDecayLR(LrScheduler):
    """This operator creates a Cosine decayed learning rate scheduler.

    Before the decay_steps are specified by user, the learning rate will be updated as:

    .. math::

        & cos\\_decay = 0.5*(1+cos(\\pi*\\frac{current\\_step}{decay\\_steps}))

        & decay\\_factor = (1-\\alpha)*cos\\_decay+\\alpha

        & learning\\_rate = base\\_learning\\_rate*decay\\_factor

    After the decay_steps specified by user, the learning rate will be :

    .. math::

        learning\\_rate = {base\\_learning\\_rate}*{\\alpha}

    It has been proposed in
    `SGDR: Stochastic Gradient Descent with Warm Restarts`_. Note that this only
    implements the cosine annealing part of SGDR, and not the restarts.

    Args:
        optimizer(Optimizer): Wrapped optimizer.
        decay_steps (int): The decay steps in the scheduler.
        alpha (float, optional): The learning rate scale factor (:math:`\\alpha`). (default: 0.0)
        last_step (int, optional): The index of last step. (default: -1)
        verbose (bool, optional): If ``True``, prints a message to stdout for each update. (default: ``False``)

    For example:

    .. code-block:: python

        import oneflow as flow

        ...
        cosine_decay_lr = flow.optim.lr_scheduler.CosineDecayLR(optimizer, decay_steps=100, alpha=0.0)
        for epoch in range(num_epoch):
            train(...)
            cosine_decay_lr.step()

    .. _SGDR\\: Stochastic Gradient Descent with Warm Restarts:
        https://arxiv.org/abs/1608.03983
    """

    def __init__(
        self,
        optimizer,
        decay_steps: int,
        alpha: float = 0.0,
        last_step=-1,
        verbose=False,
    ):
        assert (
            decay_steps > 0
        ), f"decay_steps must greater than zero, but got {decay_steps}"
        self.decay_steps = decay_steps
        self.alpha = alpha
        super().__init__(optimizer, last_step, verbose)

    def get_lr(self):
        if self.last_step < self.decay_steps:
            cos_decay = 0.5 * (
                1 + math.cos(math.pi * self.last_step / self.decay_steps)
            )
            decay_factor = (1 - self.alpha) * cos_decay + self.alpha
            return [base_lr * decay_factor for base_lr in self.base_lrs]
        else:
            return [base_lr * self.alpha for base_lr in self.base_lrs]

    def generate_conf_for_graph(self, opt_confs):
        # CosineDecayLR is the same as CosineDecayConf in nn.Graph
        for opt_conf in opt_confs:
            learning_rate_decay_conf = opt_conf.mutable_learning_rate_decay()
            learning_rate_decay_conf.mutable_cosine_conf().set_decay_batches(
                self.decay_steps
            )
            learning_rate_decay_conf.mutable_cosine_conf().set_alpha(self.alpha)
