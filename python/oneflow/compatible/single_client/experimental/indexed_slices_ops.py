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
from typing import Optional, Tuple

import oneflow._oneflow_internal
from oneflow.compatible import single_client as flow
from oneflow.compatible.single_client.framework import distribute as distribute_util
from oneflow.compatible.single_client.framework import id_util as id_util
from oneflow.compatible.single_client.framework import input_blob_def as input_blob_util
from oneflow.compatible.single_client.framework import interpret_util as interpret_util
from oneflow.compatible.single_client.framework import remote_blob as remote_blob_util
from oneflow.core.operator import op_conf_pb2 as op_conf_util
from oneflow.core.register import logical_blob_id_pb2 as logical_blob_id_util


def indexed_slices_reduce_sum(
    indices: input_blob_util.ArgBlobDef,
    values: input_blob_util.ArgBlobDef,
    name: Optional[str] = None,
) -> Tuple[oneflow._oneflow_internal.BlobDesc]:
    op = (
        flow.user_op_builder(
            name if name is not None else id_util.UniqueStr("IndexedSlicesReduceSum_")
        )
        .Op("indexed_slices_reduce_sum")
        .Input("x_indices", [indices])
        .Input("x_values", [values])
        .Output("y_indices")
        .Output("y_values")
        .Output("num_unique")
        .Build()
    )
    return op.InferAndTryRun().RemoteBlobList()
