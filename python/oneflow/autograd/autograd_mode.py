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

import oneflow._oneflow_internal
from oneflow._oneflow_internal.autograd import AutoGradMode


def is_grad_enabled():
    r"""
    Returns True if grad mode is currently enabled.
    """
    return oneflow._oneflow_internal.autograd.is_grad_enabled()


class inference_mode:
    r"""
    Context-manager that enables or disables inference mode

    InferenceMode is a new context manager analogous to no_grad to be used when you arecertain
    your operations will have no interactions with autograd (e.g., model training). Code run
    under this mode gets better performance by disabling view tracking and version counter bumps.

    This context manager is thread local; it will not affect computation in other threads.

    Also functions as a decorator. (Make sure to instantiate with parenthesis.)

    Args:
        mode (bool): Flag whether to enable or disable inference mode. (default: True)

    .. code-block:: python

        >>> import oneflow as flow
        >>> x = flow.ones(2, 3, requires_grad=True)
        >>> with flow.inference_mode():
        ...     y = x * x
        >>> y.requires_grad
        False
        >>> @flow.inference_mode()
        ... def no_grad_func(x):
        ...     return x * x
        >>> y = no_grad_func(x)
        >>> y.requires_grad
        False
    """

    def __init__(self, mode=True):
        self.infer_mode = mode

    def __call__(self, func):
        def warpper(*args, **kwargs):
            with AutoGradMode(not self.infer_mode):
                return func(*args, **kwargs)

        return warpper

    def __enter__(self):
        self.grad_mode = AutoGradMode(not self.infer_mode)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass


class grad_enable:
    r"""
    Context-manager that enabled gradient calculation.

    Enables gradient calculation, if it has been disabled via no_grad.

    This context manager is thread local; it will not affect computation in other threads.

    Also functions as a decorator. (Make sure to instantiate with parenthesis.)

    .. code-block:: python

        >>> import oneflow as flow
        >>> x = flow.ones(2, 3, requires_grad=True)
        >>> with flow.no_grad():
        ...     with flow.grad_enable():
        ...         y = x * x
        >>> y.requires_grad
        True
        >>> @flow.grad_enable()
        ... def no_grad_func(x):
        ...     return x * x
        >>> with flow.no_grad():
        ...     y = no_grad_func(x)
        >>> y.requires_grad
        True
    """

    def __call__(self, func):
        def warpper(*args, **kwargs):
            with AutoGradMode(True):
                return func(*args, **kwargs)

        return warpper

    def __enter__(self):
        self.grad_mode = AutoGradMode(True)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass


class no_grad:
    r"""
    Context-manager that disabled gradient calculation.

    Disabling gradient calculation is useful for inference, when you are sure that
    you will not call Tensor.backward(). It will reduce memory consumption for computations
    that would otherwise have requires_grad=True.

    In this mode, the result of every computation will have requires_grad=False, even when
    the inputs have requires_grad=True.

    This context manager is thread local; it will not affect computation in other threads.

    Also functions as a decorator. (Make sure to instantiate with parenthesis.)

    .. code-block:: python

        >>> import oneflow as flow
        >>> x = flow.ones(2, 3, requires_grad=True)
        >>> with flow.no_grad():
        ...     y = x * x
        >>> y.requires_grad
        False
        >>> @flow.no_grad()
        ... def no_grad_func(x):
        ...     return x * x
        >>> y = no_grad_func(x)
        >>> y.requires_grad
        False
    """

    def __call__(self, func):
        def warpper(*args, **kwargs):
            with AutoGradMode(False):
                return func(*args, **kwargs)

        return warpper

    def __enter__(self):
        self.grad_mode = AutoGradMode(False)
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass


if __name__ == "__main__":
    import doctest

    doctest.testmod(raise_on_error=True)
