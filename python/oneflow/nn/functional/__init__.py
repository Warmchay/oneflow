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
from oneflow.nn.modules.interpolate import interpolate
from oneflow.nn.modules.norm import l2_normalize
from oneflow.nn.modules.affine_grid import affine_grid
from oneflow.nn.modules.grid_sample import grid_sample
from oneflow._C import conv1d
from oneflow._C import conv2d
from oneflow._C import conv3d
from oneflow._C import avgpool_1d
from oneflow._C import avgpool_2d
from oneflow._C import avgpool_3d
from oneflow._C import maxpool_1d
from oneflow._C import maxpool_2d
from oneflow._C import maxpool_3d
from oneflow._C import adaptive_avg_pool1d
from oneflow._C import adaptive_avg_pool2d
from oneflow._C import adaptive_avg_pool3d
from oneflow._C import relu
from oneflow._C import hardtanh
from oneflow._C import hardsigmoid
from oneflow._C import hardswish
from oneflow._C import leaky_relu
from oneflow._C import elu
from oneflow._C import selu
from oneflow._C import sigmoid
from oneflow._C import prelu
from oneflow._C import gelu
from oneflow._C import log_sigmoid as logsigmoid
from oneflow._C import log_sigmoid
from oneflow._C import softsign
from oneflow._C import softmax
from oneflow._C import softplus
from oneflow._C import tanh
from oneflow._C import silu
from oneflow._C import mish
from oneflow._C import layer_norm
from oneflow._C import dropout
from oneflow._C import smooth_l1_loss
from oneflow._C import pad
from oneflow._C import upsample
from oneflow.nn.modules.one_hot import one_hot
