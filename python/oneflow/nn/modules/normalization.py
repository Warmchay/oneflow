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
from typing import Tuple, Union

import oneflow as flow
from oneflow.framework.tensor import Tensor
from oneflow.nn import init
from oneflow.nn.module import Module

_shape_t = Union[int, Tuple[int], flow._oneflow_internal.Size]


class GroupNorm(Module):
    """The interface is consistent with PyTorch.
    The documentation is referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.GroupNorm.html

    Applies Group Normalization over a mini-batch of inputs as described in
    the paper `Group Normalization <https://arxiv.org/abs/1803.08494>`__

    .. math::

        y = \\frac{x - \\mathrm{E}[x]}{ \\sqrt{\\mathrm{Var}[x] + \\epsilon}} * \\gamma + \\beta

    The input channels are separated into :attr:`num_groups` groups, each containing
    ``num_channels / num_groups`` channels. The mean and standard-deviation are calculated
    separately over the each group. :math:`\\gamma` and :math:`\\beta` are learnable
    per-channel affine transform parameter vectors of size :attr:`num_channels` if
    :attr:`affine` is ``True``.
    The standard-deviation is calculated via the biased estimator, equivalent to
    `torch.var(input, unbiased=False)`.

    This layer uses statistics computed from input data in both training and
    evaluation modes.

    Args:
        num_groups (int): number of groups to separate the channels into
        num_channels (int): number of channels expected in input
        eps: a value added to the denominator for numerical stability. Default: 1e-5
        affine: a boolean value that when set to ``True``, this module
            has learnable per-channel affine parameters initialized to ones (for weights)
            and zeros (for biases). Default: ``True``.

    Shape:
        - Input: :math:`(N, C, *)` where :math:`C=\\text{num_channels}`
        - Output: :math:`(N, C, *)` (same shape as input)

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        >>> input = flow.Tensor(np.random.randn(20, 6, 10, 10))
        >>> # Separate 6 channels into 3 groups
        >>> m = flow.nn.GroupNorm(3, 6)
        >>> # Separate 6 channels into 6 groups (equivalent with InstanceNorm)
        >>> m = flow.nn.GroupNorm(6, 6)
        >>> # Put all 6 channels into a single group (equivalent with LayerNorm)
        >>> m = flow.nn.GroupNorm(1, 6)
        >>> # Activating the module
        >>> output = m(input)
    
"""

    def __init__(
        self,
        num_groups: int,
        num_channels: int,
        eps: float = 1e-05,
        affine: bool = True,
    ) -> None:
        super().__init__()
        assert num_groups > 0, "The num_groups must larger than zero"
        assert num_channels > 0, "The num_channels must larger than zero"
        self.num_groups = num_groups
        self.num_channels = num_channels
        self.eps = eps
        self.affine = affine
        if self.affine:
            self.weight = flow.nn.Parameter(flow.Tensor(1, num_channels, 1))
            self.bias = flow.nn.Parameter(flow.Tensor(1, num_channels, 1))
        else:
            self.register_parameter("weight", None)
            self.register_parameter("bias", None)
        self.reset_parameters()

    def reset_parameters(self) -> None:
        if self.affine:
            flow.nn.init.ones_(self.weight)
            flow.nn.init.zeros_(self.bias)

    def forward(self, input: Tensor) -> Tensor:
        assert (
            len(input.shape) >= 3
        ), "The dimensions of input tensor must larger than 2"
        assert (
            input.shape[1] == self.num_channels
        ), "The channels of input tensor must equal num_channels"
        origin_shape = input.shape
        reshape_to_1d = flow.reshape(
            input, shape=[origin_shape[0], self.num_groups, -1]
        )
        mean = flow.mean(reshape_to_1d, dim=2, keepdim=True)
        variance = flow.var(reshape_to_1d, dim=2, unbiased=False, keepdim=True)
        normalized = (reshape_to_1d - mean) / flow.sqrt(variance + self.eps)
        normalized = flow.reshape(
            normalized, shape=[origin_shape[0], self.num_channels, -1]
        )
        if self.weight is not None:
            normalized = normalized * self.weight
        if self.bias is not None:
            normalized = normalized + self.bias
        res = flow.reshape(normalized, shape=tuple(input.shape))
        return res

    def extra_repr(self) -> str:
        return "{num_groups}, {num_channels}, eps={eps}, affine={affine}".format(
            **self.__dict__
        )


class LayerNorm(Module):
    """Applies Layer Normalization over a mini-batch of inputs as described in
    the paper `Layer Normalization <https://arxiv.org/abs/1607.06450>`__

    .. math::
        y = \\frac{x - \\mathrm{E}[x]}{ \\sqrt{\\mathrm{Var}[x] + \\epsilon}} * \\gamma + \\beta

    The mean and standard-deviation are calculated separately over the last
    certain number dimensions which have to be of the shape specified by
    :attr:`normalized_shape`.
    :math:`\\gamma` and :math:`\\beta` are learnable affine transform parameters of
    :attr:`normalized_shape` if :attr:`elementwise_affine` is ``True``.
    The standard-deviation is calculated via the biased estimator.

    .. note::
        Unlike Batch Normalization and Instance Normalization, which applies
        scalar scale and bias for each entire channel/plane with the
        :attr:`affine` option, Layer Normalization applies per-element scale and
        bias with :attr:`elementwise_affine`.

    This layer uses statistics computed from input data in both training and
    evaluation modes.

    Args:
        normalized_shape (int or list or oneflow.Size): input shape from an expected input of size

            .. math::
                [* \\times \\text{normalized_shape}[0] \\times \\text{normalized_shape}[1] \\times \\ldots \\times \\text{normalized_shape}[-1]]

            If a single integer is used, it is treated as a singleton list, and this module will

            normalize over the last dimension which is expected to be of that specific size.

        eps: a value added to the denominator for numerical stability. Default: 1e-5
        elementwise_affine: a boolean value that when set to ``True``, this module
            has learnable per-element affine parameters initialized to ones (for weights)
            and zeros (for biases). Default: ``True``.

    Shape:
        - Input: :math:`(N, *)`
        - Output: :math:`(N, *)` (same shape as input)

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input_arr = np.array(
        ...     [
        ...         [
        ...             [[-0.16046895, -1.03667831], [-0.34974465, 0.26505867]],
        ...             [[-1.24111986, -0.53806001], [1.72426331, 0.43572459]],
        ...         ],
        ...         [
        ...             [[-0.77390957, -0.42610624], [0.16398858, -1.35760343]],
        ...             [[1.07541728, 0.11008703], [0.26361224, -0.48663723]],
        ...         ],
        ...     ],
        ...     dtype=np.float32,
        ... )

        >>> x = flow.Tensor(input_arr)
        >>> m = flow.nn.LayerNorm(2)
        >>> y = m(x).numpy()
        >>> y
        array([[[[ 0.99997395, -0.99997395],
                 [-0.999947  ,  0.999947  ]],
        <BLANKLINE>
                [[-0.99995965,  0.9999595 ],
                 [ 0.999988  , -0.999988  ]]],
        <BLANKLINE>
        <BLANKLINE>
               [[[-0.9998348 ,  0.99983466],
                 [ 0.9999914 , -0.9999914 ]],
        <BLANKLINE>
                [[ 0.9999785 , -0.9999785 ],
                 [ 0.9999645 , -0.9999645 ]]]], dtype=float32)

    """

    __constants__ = ["normalized_shape", "eps", "elementwise_affine"]
    normalized_shape: Tuple[int, ...]
    eps: float
    elementwise_affine: bool

    def __init__(
        self,
        normalized_shape: _shape_t,
        eps: float = 1e-05,
        elementwise_affine: bool = True,
    ) -> None:
        super(LayerNorm, self).__init__()
        if isinstance(normalized_shape, int):
            normalized_shape = (normalized_shape,)
        self.normalized_shape = tuple(normalized_shape)
        self.eps = eps
        self.elementwise_affine = elementwise_affine
        if self.elementwise_affine:
            self.weight = flow.nn.Parameter(flow.Tensor(*self.normalized_shape))
            self.bias = flow.nn.Parameter(flow.Tensor(*self.normalized_shape))
        else:
            self.register_parameter("weight", None)
            self.register_parameter("bias", None)
        self.reset_parameters()
        self.begin_norm_axis = 1
        self.begin_params_axis = 1

    def reset_parameters(self) -> None:
        if self.elementwise_affine:
            init.ones_(self.weight)
            init.zeros_(self.bias)

    def forward(self, x):
        assert len(x.shape) > len(
            self.normalized_shape
        ), "Input tensor dim must greater than normalized dim!"
        self.begin_norm_axis = len(x.shape) - len(self.normalized_shape)
        self.begin_params_axis = len(x.shape) - len(self.normalized_shape)
        if x.device == flow.device("cpu"):
            reduce_axis = []
            for dim in range(len(x.shape)):
                if dim >= self.begin_norm_axis:
                    reduce_axis.append(dim)
            mean = x.mean(dim=reduce_axis, keepdim=True)
            variance = x.var(dim=reduce_axis, unbiased=False, keepdim=True)
            params_shape = x.shape[self.begin_params_axis :]
            weight = self.weight
            bias = self.bias
            if len(mean.shape) == 1:
                nd_params_shape = [1] * len(x.shape)
                nd_params_shape[self.begin_norm_axis] = params_shape[0]
                mean = flow.reshape(mean, shape=nd_params_shape)
                variance = flow.reshape(variance, nd_params_shape)
                if self.weight and params_shape[0] == self.weight.nelement():
                    weight = flow.reshape(self.weight, shape=nd_params_shape)
                if self.bias and params_shape[0] == self.bias.nelement():
                    bias = flow.reshape(self.bias, shape=nd_params_shape)
            elif len(mean.shape) == len(x.shape):
                pass
            else:
                raise ValueError(
                    "shape of mean and variance should be 1D or has number of axes and x's"
                )
            variance += self.eps
            normalized = (x - mean) * variance.rsqrt()
            if self.weight is not None:
                normalized = normalized * weight
            if self.bias is not None:
                normalized = normalized + bias
            affined = normalized
            nd_params_shape = [1] * (len(x.shape) - len(params_shape)) + list(
                params_shape
            )
            if self.elementwise_affine:
                affined = affined * self.weight
                affined = affined + self.bias
            return affined
        else:
            if self.elementwise_affine:
                res = flow._C.layer_norm_affine(
                    x,
                    self.weight,
                    self.bias,
                    begin_norm_axis=self.begin_norm_axis,
                    begin_params_axis=self.begin_params_axis,
                    epsilon=self.eps,
                )
            else:
                res = flow._C.layer_norm(
                    x,
                    begin_norm_axis=self.begin_norm_axis,
                    begin_params_axis=self.begin_params_axis,
                    epsilon=self.eps,
                )
            return res

    def extra_repr(self) -> str:
        return "{normalized_shape}, eps={eps}, elementwise_affine={elementwise_affine}".format(
            **self.__dict__
        )


if __name__ == "__main__":
    import doctest

    doctest.testmod(raise_on_error=True)
