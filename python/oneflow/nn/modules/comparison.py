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
import oneflow as flow
from oneflow.framework.tensor import register_tensor_op


@register_tensor_op("eq")
def eq_op(input, other):
    """
    Computes element-wise equality.
    The second argument can be a number or a tensor whose shape is broadcastable with the first argument.

    Args:
        input (oneflow.Tensor): the tensor to compare
        other (oneflow.Tensor, float or int): the target to compare

    Returns:

        - A boolean tensor that is True where :attr:`input` is equal to :attr:`other` and False elsewhere

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        
        >>> input = flow.Tensor(np.array([2, 3, 4, 5]), dtype=flow.float32)
        >>> other = flow.Tensor(np.array([2, 3, 4, 1]), dtype=flow.float32)

        >>> y = flow.eq(input, other)
        >>> y
        tensor([1, 1, 1, 0], dtype=oneflow.int8)

    """
    return flow._C.equal(input, other)


@register_tensor_op("ne")
def ne_op(input, other):
    """
    Computes element-wise not equality.
    The second argument can be a number or a tensor whose shape is broadcastable with the first argument.

    Args:
        input (oneflow.Tensor): the tensor to compare
        other (oneflow.Tensor, float or int): the target to compare

    Returns:

        - A boolean tensor that is True where :attr:`input` is not equal to :attr:`other` and False elsewhere

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        
        >>> input = flow.Tensor(np.array([2, 3, 4, 5]), dtype=flow.float32)
        >>> other = flow.Tensor(np.array([2, 3, 4, 1]), dtype=flow.float32)

        >>> y = flow.ne(input, other)
        >>> y
        tensor([0, 0, 0, 1], dtype=oneflow.int8)

    """
    return flow._C.not_equal(input, other)


def greater_op(input, other):
    """Returns the truth value of :math:`input > other` element-wise.

    Args:
        input (oneflow.Tensor): A Tensor
        other (oneflow.Tensor): A Tensor

    Returns:
        oneflow.Tensor: A Tensor with int8 type.

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input1 = flow.Tensor(np.random.randn(2, 6, 5, 3), dtype=flow.float32)
        >>> input2 = flow.Tensor(np.random.randn(2, 6, 5, 3), dtype=flow.float32)

        >>> out = flow.gt(input1, input2).shape
        >>> out
        flow.Size([2, 6, 5, 3])

    """
    return flow._C.greater(input, other)


@register_tensor_op("gt")
def greater_op_tensor(input, other):
    """

    gt() -> Tensor

    See :func:`oneflow.gt`

    """
    return greater_op(input, other)


def greater_equal_op(input, other):
    """Returns the truth value of :math:`input >= other` element-wise.

    Args:
        input (oneflow.Tensor): A Tensor
        other (oneflow.Tensor): A Tensor

    Returns:
        oneflow.Tensor: A Tensor with int8 type.

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input1 = flow.Tensor(np.array([1, 2, 3]).astype(np.float32), dtype=flow.float32)
        >>> input2 = flow.Tensor(np.array([1, 1, 4]).astype(np.float32), dtype=flow.float32)

        >>> out = flow.ge(input1, input2)
        >>> out
        tensor([1, 1, 0], dtype=oneflow.int8)

    """
    return flow._C.greater_equal(input, other)


@register_tensor_op("ge")
def greater_equal_op_tensor(input, other):
    """

    ge() -> Tensor

    See :func:`oneflow.ge`

    """
    return greater_equal_op(input, other)


@register_tensor_op("lt")
def less_op(input, other):
    """Returns the truth value of :math:`input < other` element-wise.

    Args:
        input (oneflow.Tensor): A Tensor
        other (oneflow.Tensor): A Tensor

    Returns:
        oneflow.Tensor: A Tensor with int8 type.

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input1 = flow.Tensor(np.array([1, 2, 3]).astype(np.float32), dtype=flow.float32)
        >>> input2 = flow.Tensor(np.array([1, 2, 4]).astype(np.float32), dtype=flow.float32)

        >>> out = flow.lt(input1, input2)
        >>> out
        tensor([0, 0, 1], dtype=oneflow.int8)

    """
    return flow._C.less(input, other)


@register_tensor_op("le")
def less_equal_op(input, other):
    """Returns the truth value of :math:`input <= other` element-wise.

    Args:
        input (oneflow.Tensor): A Tensor
        other (oneflow.Tensor): A Tensor

    Returns:
        oneflow.Tensor: A Tensor with int8 type.

    For example:

    .. code-block:: python

        >>> import numpy as np
        >>> import oneflow as flow
        
        >>> input1 = flow.Tensor(np.array([1, 2, 3]).astype(np.float32), dtype=flow.float32)
        >>> input2 = flow.Tensor(np.array([1, 1, 4]).astype(np.float32), dtype=flow.float32)

        >>> out = flow.le(input1, input2)
        >>> out
        tensor([1, 0, 1], dtype=oneflow.int8)

    """
    return flow._C.less_equal(input, other)


@register_tensor_op("ne")
def ne_op(input, other):
    """
    Computes element-wise not equality.
    The second argument can be a number or a tensor whose shape is broadcastable with the first argument.

    Args:
        input (oneflow.Tensor): the tensor to compare
        other (oneflow.Tensor, float or int): the target to compare

    Returns:

        - A boolean tensor that is True where :attr:`input` is not equal to :attr:`other` and False elsewhere

    For example:

    .. code-block:: python

        >>> import oneflow as flow
        >>> import numpy as np
        
        >>> input = flow.Tensor(np.array([2, 3, 4, 5]), dtype=flow.float32)
        >>> other = flow.Tensor(np.array([2, 3, 4, 1]), dtype=flow.float32)

        >>> y = flow.ne(input, other)
        >>> y
        tensor([0, 0, 0, 1], dtype=oneflow.int8)

    """
    return flow._C.not_equal(input, other)


if __name__ == "__main__":
    import doctest

    doctest.testmod(raise_on_error=True)
