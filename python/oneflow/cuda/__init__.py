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


def is_available() -> bool:
    r"""Returns a bool indicating if CUDA is currently available."""
    if not hasattr(flow._oneflow_internal, "CudaGetDeviceCount"):
        return False
    # This function never throws and returns 0 if driver is missing or can't
    # be initialized
    return flow._oneflow_internal.CudaGetDeviceCount() > 0
