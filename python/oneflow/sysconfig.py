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
from oneflow.framework.sysconfig import (
    cmake_build_type,
    get_compile_flags,
    get_include,
    get_lib,
    get_link_flags,
    has_rpc_backend_grpc,
    has_rpc_backend_local,
    with_cuda,
    with_xla,
    with_rdma,
)
