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
from typing import Union, Sequence

import oneflow as flow
from oneflow.nn.module import Module
from oneflow.nn.common_types import _size_4_t
from oneflow.nn.modules.utils import _quadruple


def pad(
    input,
    paddings: Sequence[int],
    constant_value: Union[int, float] = 0):
    padding_before = []
    padding_after = []
    if isinstance(paddings, (list, tuple)):
        assert len(paddings) == len(x.shape), ValueError(
            "paddings must be the same size of input dims"
        )
        for p in paddings:
            assert isinstance(p, (list, tuple)) and len(p) == 2, ValueError(
                "the elem of paddings must be a tuple or a list with length of 2"
            )
            padding_before.append(p[0])
            padding_after.append(p[1])
    else:
        raise ValueError("paddings must be a tuple or a list.")
    if input.dtype in [flow.float32, flow.float16, flow.float64]:
        floating_constant_value = float(constant_value)
        integral_constant_value = int(0)
    else:
        floating_constant_value = float(0)
        integral_constant_value = int(constant_value)
    
    _op = (
        flow.builtin_op("pad")
        .Input("x")
        .Output("y")
        .Attr("padding_before", padding_before)
        .Attr("padding_after", padding_after)
        .Attr("floating_constant_value", floating_constant_value)
        .Attr("integral_constant_value", integral_constant_value)
        .Build()
    )

    return _op(input)[0]



class ReplicationPad2d(Module):
    """The interface is consistent with PyTorch.
    The documentation is referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.ReplicationPad2d.html?highlight=replicationpad2d#torch.nn.ReplicationPad2d

    Pads the input tensor using the replication of the input boundary.

    Args:
        padding (Union[int, tuple, list]):  the size of the padding. If is `int`, uses the same padding in all boundaries. If a 4-`tuple`, uses (:math:`\\mathrm{padding_{left}}`, :math:`\\mathrm{padding_{right}}`, :math:`\\mathrm{padding_{top}}`, :math:`\\mathrm{padding_{bottom}}`)

    Shape:
        - Input: :math:`(N, C, H_{in}, W_{in})`
        - Output: :math:`(N, C, H_{out}, W_{out})` where

            :math:`H_{out} = H_{in} + \\mathrm{padding_{top}} + \\mathrm{padding_{bottom}}`

            :math:`W_{out} = W_{in} + \\mathrm{padding_{left}} + \\mathrm{padding_{right}}`

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        >>> replicationpad_layer_0 = flow.nn.ReplicationPad2d((2, 2, 1, 1))
        >>> input = flow.Tensor(np.arange(18).reshape((1, 2, 3, 3)).astype(np.float32))
        >>> input_int = flow.Tensor(np.arange(18).reshape((1, 2, 3, 3)).astype(np.int32))
        >>> output = replicationpad_layer_0(input)
        >>> output.shape
        flow.Size([1, 2, 5, 7])
        >>> output
        tensor([[[[ 0.,  0.,  0.,  1.,  2.,  2.,  2.],
                  [ 0.,  0.,  0.,  1.,  2.,  2.,  2.],
                  [ 3.,  3.,  3.,  4.,  5.,  5.,  5.],
                  [ 6.,  6.,  6.,  7.,  8.,  8.,  8.],
                  [ 6.,  6.,  6.,  7.,  8.,  8.,  8.]],
        <BLANKLINE>
                 [[ 9.,  9.,  9., 10., 11., 11., 11.],
                  [ 9.,  9.,  9., 10., 11., 11., 11.],
                  [12., 12., 12., 13., 14., 14., 14.],
                  [15., 15., 15., 16., 17., 17., 17.],
                  [15., 15., 15., 16., 17., 17., 17.]]]], dtype=oneflow.float32)
        >>> output_int = replicationpad_layer_0(input_int)
        >>> output_int
        tensor([[[[ 0.,  0.,  0.,  1.,  2.,  2.,  2.],
                  [ 0.,  0.,  0.,  1.,  2.,  2.,  2.],
                  [ 3.,  3.,  3.,  4.,  5.,  5.,  5.],
                  [ 6.,  6.,  6.,  7.,  8.,  8.,  8.],
                  [ 6.,  6.,  6.,  7.,  8.,  8.,  8.]],
        <BLANKLINE>
                 [[ 9.,  9.,  9., 10., 11., 11., 11.],
                  [ 9.,  9.,  9., 10., 11., 11., 11.],
                  [12., 12., 12., 13., 14., 14., 14.],
                  [15., 15., 15., 16., 17., 17., 17.],
                  [15., 15., 15., 16., 17., 17., 17.]]]], dtype=oneflow.float32)

    """

    def __init__(self, padding: _size_4_t):
        super().__init__()
        if isinstance(padding, (tuple, list)):
            assert len(padding) == 4, ValueError("Length of padding must be 4")
            boundary = [*padding]
        elif isinstance(padding, int):
            boundary = _quadruple(padding)
        else:
            raise ValueError("padding must be int or list or tuple!")
        self.padding = boundary

    def forward(self, x):
        return flow.F.pad(x, pad=self.padding, mode="replicate")

    def extra_repr(self) -> str:
        return "{}".format(self.padding)


class ReflectionPad2d(Module):
    """The interface is consistent with PyTorch.
    The documentation is referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.ReflectionPad2d.html


    This operator pads the input tensor using the reflection of the input boundary.

    Args:
        padding (Union[int,tuple]): The size or bundary of padding, if is `int` uses the same padding in all dimension; if 4-dims `tuple`, uses :math:`(\\text{padding}_{\\text{left}}, \\text{padding}_{\\text{right}}, \\text{padding}_{\\text{top}}, \\text{padding}_{\\text{bottom}} )`

    Returns:
        Tensor: Returns a new tensor which is result of the reflection padding of the input tensor.

    Shape:
        - Input: :math:`(N, C, H_{\\text{in}}, W_{\\text{in}})`
        - Output: :math:`(N, C, H_{\\text{out}}, W_{\\text{out}})` where

          :math:`H_{\\text{out}} = H_{\\text{in}} + \\text{padding}_{\\text{top}} + \\text{padding}_{\\text{bottom}}`

          :math:`W_{\\text{out}} = W_{\\text{in}} + \\text{padding}_{\\text{left}} + \\text{padding}_{\\text{right}}`

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        >>> input = flow.Tensor(np.arange(18).reshape((1, 2, 3, 3)), dtype=flow.float32)
        >>> m = flow.nn.ReflectionPad2d((2, 2, 1, 1))
        >>> out = m(input)
        >>> out
        tensor([[[[ 5.,  4.,  3.,  4.,  5.,  4.,  3.],
                  [ 2.,  1.,  0.,  1.,  2.,  1.,  0.],
                  [ 5.,  4.,  3.,  4.,  5.,  4.,  3.],
                  [ 8.,  7.,  6.,  7.,  8.,  7.,  6.],
                  [ 5.,  4.,  3.,  4.,  5.,  4.,  3.]],
        <BLANKLINE>
                 [[14., 13., 12., 13., 14., 13., 12.],
                  [11., 10.,  9., 10., 11., 10.,  9.],
                  [14., 13., 12., 13., 14., 13., 12.],
                  [17., 16., 15., 16., 17., 16., 15.],
                  [14., 13., 12., 13., 14., 13., 12.]]]], dtype=oneflow.float32)

    """

    def __init__(self, padding: _size_4_t) -> None:
        super().__init__()
        if isinstance(padding, tuple):
            assert len(padding) == 4, ValueError("Padding length must be 4")
            boundary = [*padding]
        elif isinstance(padding, int):
            boundary = _quadruple(padding)
        else:
            raise ValueError("padding must be in or list or tuple!")
        self.padding = boundary

    def forward(self, x):
        (H, W) = (x.shape[2], x.shape[3])
        if (
            self.padding[2] < H
            and self.padding[3] < H
            and (self.padding[0] < W)
            and (self.padding[1] < W)
        ):
            return flow.F.pad(x, pad=self.padding, mode="reflect")
        else:
            raise ValueError(
                "padding size should be less than the corresponding input dimension!"
            )

    def extra_repr(self) -> str:
        return "{}".format(self.padding)


class ConstantPad1d(Module):
    """Pads the input tensor boundaries with a constant value.
    The interface is consistent with PyTorch, and referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.ConstantPad1d.html?highlight=constantpad1d#torch.nn.ConstantPad1d

    For `N`-dimensional padding, use :func:`torch.nn.functional.pad()`.

    Args:
        padding (int, list, tuple): the size of the padding. If is `int`, uses the same
            padding in both boundaries. If a 2-`tuple`, uses
            (:math:`\\text{padding_left}`, :math:`\\text{padding_right}`)

        value (int, float): The constant value used for padding. Defaults to 0.

    Shape:
        - Input: :math:`(N, C, W_{in})`
        - Output: :math:`(N, C, W_{out})` where

          :math:`W_{out} = W_{in} + \\text{padding\\_left} + \\text{padding\\_right}`

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np

        >>> input = flow.tensor(np.arange(8).reshape(2,2,2).astype(np.float32))
        >>> m = flow.nn.ConstantPad1d(padding=[1, 2], value=9.9999)
        >>> output = m(input)
        >>> output
        tensor([[[9.9999, 0.0000, 1.0000, 9.9999, 9.9999],
                 [9.9999, 2.0000, 3.0000, 9.9999, 9.9999]],
        <BLANKLINE>
                [[9.9999, 4.0000, 5.0000, 9.9999, 9.9999],
                 [9.9999, 6.0000, 7.0000, 9.9999, 9.9999]]], dtype=oneflow.float32)

    """

    def __init__(self, padding: Union[int, tuple, list], value: Union[int, float] = 0):
        super().__init__()
        if isinstance(padding, (tuple, list)):
            assert len(padding) == 2, ValueError("Length of padding must be 4")
            boundary = [padding[0], padding[1]]
        elif isinstance(padding, int):
            boundary = [padding, padding]
        else:
            raise ValueError("padding must be int or list or tuple!")
        self.padding = boundary
        self.value = value

    def forward(self, x):
        if x.dtype in (flow.float32, flow.float16, flow.float64):
            self.value = float(self.value)
        else:
            self.value = int(self.value)
        return flow.F.pad(x, pad=self.padding, mode="constant", value=self.value)


class ConstantPad2d(Module):
    """The interface is consistent with PyTorch.
    The documentation is referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.ConstantPad2d.html?highlight=constantpad2d#torch.nn.ConstantPad2d

    This operator pads the input with constant value that user specifies.
    User can set the amount of padding by setting the parameter `paddings`.

    Args:
        padding (int, tuple, list):  the size of the padding.
            If is `int`, uses the same padding in all boundaries.
            If a 4-`tuple`, uses
            (:math:`\\mathrm{padding_{left}}`, :math:`\\mathrm{padding_{right}}`, :math:`\\mathrm{padding_{top}}`, :math:`\\mathrm{padding_{bottom}}`)

        value (int, float): The constant value used for padding. Defaults to 0.

    Shape:
        - Input: :math:`(N, C, H_{in}, W_{in})`
        - Output: :math:`(N, C, H_{out}, W_{out})` where

          :math:`H_{out} = H_{in} + \\mathrm{padding_{top}} + \\mathrm{padding_{bottom}}`
          :math:`W_{out} = W_{in} + \\mathrm{padding_{left}} + \\mathrm{padding_{right}}`

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np

        >>> constantpad_layer_0 = flow.nn.ConstantPad2d((2, 2, 1, 1), 1)
        >>> input = flow.Tensor(np.arange(18).reshape((1, 2, 3, 3)).astype(np.float32))
        >>> input_int = flow.Tensor(np.arange(18).reshape((1, 2, 3, 3)).astype(np.int32))
        >>> output = constantpad_layer_0(input)
        >>> output.shape
        flow.Size([1, 2, 5, 7])
        >>> output
        tensor([[[[ 1.,  1.,  1.,  1.,  1.,  1.,  1.],
                  [ 1.,  1.,  0.,  1.,  2.,  1.,  1.],
                  [ 1.,  1.,  3.,  4.,  5.,  1.,  1.],
                  [ 1.,  1.,  6.,  7.,  8.,  1.,  1.],
                  [ 1.,  1.,  1.,  1.,  1.,  1.,  1.]],
        <BLANKLINE>
                 [[ 1.,  1.,  1.,  1.,  1.,  1.,  1.],
                  [ 1.,  1.,  9., 10., 11.,  1.,  1.],
                  [ 1.,  1., 12., 13., 14.,  1.,  1.],
                  [ 1.,  1., 15., 16., 17.,  1.,  1.],
                  [ 1.,  1.,  1.,  1.,  1.,  1.,  1.]]]], dtype=oneflow.float32)
        >>> output_int = constantpad_layer_0(input_int)
        >>> output_int
        tensor([[[[ 1.,  1.,  1.,  1.,  1.,  1.,  1.],
                  [ 1.,  1.,  0.,  1.,  2.,  1.,  1.],
                  [ 1.,  1.,  3.,  4.,  5.,  1.,  1.],
                  [ 1.,  1.,  6.,  7.,  8.,  1.,  1.],
                  [ 1.,  1.,  1.,  1.,  1.,  1.,  1.]],
        <BLANKLINE>
                 [[ 1.,  1.,  1.,  1.,  1.,  1.,  1.],
                  [ 1.,  1.,  9., 10., 11.,  1.,  1.],
                  [ 1.,  1., 12., 13., 14.,  1.,  1.],
                  [ 1.,  1., 15., 16., 17.,  1.,  1.],
                  [ 1.,  1.,  1.,  1.,  1.,  1.,  1.]]]], dtype=oneflow.float32)

    """

    def __init__(self, padding: Union[int, tuple, list], value: Union[int, float] = 0):
        super().__init__()
        if isinstance(padding, (tuple, list)):
            assert len(padding) == 4, ValueError("Length of padding must be 4")
            boundary = [padding[0], padding[1], padding[2], padding[3]]
        elif isinstance(padding, int):
            boundary = [padding, padding, padding, padding]
        else:
            raise ValueError("padding must be int or list or tuple!")
        self.padding = boundary
        self.value = value

    def forward(self, x):
        if x.dtype in (flow.float32, flow.float16, flow.float64):
            self.value = float(self.value)
        else:
            self.value = int(self.value)
        return flow.F.pad(x, pad=self.padding, mode="constant", value=self.value)


class ConstantPad3d(Module):
    """Pads the input tensor boundaries with a constant value.
    The interface is consistent with PyTorch, and referenced from:
    https://pytorch.org/docs/stable/generated/torch.nn.ConstantPad3d.html?highlight=constantpad3d#torch.nn.ConstantPad3d

    For `N`-dimensional padding, use :func:`flow.nn.functional.pad()`.

    Args:
        padding (int, list, tuple): the size of the padding. If is `int`, uses the same
            padding in all boundaries. If a 6-`tuple`, uses
            (:math:`\\text{padding_left}`, :math:`\\text{padding_right}`,
            :math:`\\text{padding_top}`, :math:`\\text{padding_bottom}`,
            :math:`\\text{padding_front}`, :math:`\\text{padding_back}`)

        value (int, float): The constant value used for padding. Defaults to 0.

    Shape:
        - Input: :math:`(N, C, D_{in}, H_{in}, W_{in})`
        - Output: :math:`(N, C, D_{out}, H_{out}, W_{out})` where

          :math:`D_{out} = D_{in} + \\text{padding_front} + \\text{padding_back}`

          :math:`H_{out} = H_{in} + \\text{padding_top} + \\text{padding_bottom}`

          :math:`W_{out} = W_{in} + \\text{padding_left} + \\text{padding_right}`

    Examples::

        >>> import oneflow as flow
        >>> import numpy as np

        >>> input = flow.tensor(np.arange(8).reshape(1,1,2,2,2).astype(np.int32))
        >>> m = flow.nn.ConstantPad3d(padding=1, value=9)
        >>> output = m(input)
        >>> output
        tensor([[[[[9, 9, 9, 9],
                   [9, 9, 9, 9],
                   [9, 9, 9, 9],
                   [9, 9, 9, 9]],
        <BLANKLINE>
                  [[9, 9, 9, 9],
                   [9, 0, 1, 9],
                   [9, 2, 3, 9],
                   [9, 9, 9, 9]],
        <BLANKLINE>
                  [[9, 9, 9, 9],
                   [9, 4, 5, 9],
                   [9, 6, 7, 9],
                   [9, 9, 9, 9]],
        <BLANKLINE>
                  [[9, 9, 9, 9],
                   [9, 9, 9, 9],
                   [9, 9, 9, 9],
                   [9, 9, 9, 9]]]]], dtype=oneflow.int32)
    """

    def __init__(self, padding: Union[int, tuple, list], value: Union[int, float] = 0):
        super().__init__()
        if isinstance(padding, (tuple, list)):
            assert len(padding) == 6, ValueError("Length of padding must be 6")
            boundary = [
                padding[0],
                padding[1],
                padding[2],
                padding[3],
                padding[4],
                padding[5],
            ]
        elif isinstance(padding, int):
            boundary = [padding, padding, padding, padding, padding, padding]
        else:
            raise ValueError("padding must be int or list or tuple!")
        self.padding = boundary
        self.value = value

    def forward(self, x):
        if x.dtype in (flow.float32, flow.float16, flow.float64):
            self.value = float(self.value)
        else:
            self.value = int(self.value)
        return flow.F.pad(x, pad=self.padding, mode="constant", value=self.value)


if __name__ == "__main__":
    import doctest

    doctest.testmod(raise_on_error=True)
