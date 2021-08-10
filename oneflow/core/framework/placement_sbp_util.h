/*
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
*/
#ifndef ONEFLOW_CORE_FRAMEWORK_PLACEMENT_SBP_UTIL_H_
#define ONEFLOW_CORE_FRAMEWORK_PLACEMENT_SBP_UTIL_H_

#include <unordered_map>
#include "oneflow/core/common/maybe.h"
#include "oneflow/core/common/symbol.h"
#include "oneflow/core/common/decorator.h"

namespace oneflow {

class Shape;
class ParallelDesc;

namespace cfg {

class ParallelDistribution;

}

namespace one {

class ConsistentTensorMeta;

}

// 1) src_nd_sbp.sbp_parallel_size() == 1
// 2) dst_nd_sbp.sbp_parallel_size() == 1
struct NaiveBoxingTransformation {
  Symbol<ParallelDesc> parallel_desc;
  Symbol<cfg::ParallelDistribution> src_nd_sbp;
  Symbol<cfg::ParallelDistribution> dst_nd_sbp;
};

namespace private_details {

Maybe<std::vector<int64_t>> GetSelectedParallelIds(const Shape& hierarchy_shape,
                                                   const std::vector<int>& axis2is_selected,
                                                   int64_t parallel_id);

Maybe<std::tuple<std::shared_ptr<const Shape>, Symbol<cfg::ParallelDistribution>,
                 Symbol<cfg::ParallelDistribution>>>
CalcDecomposableEquivalentShapeAndNdSbpPair(const Shape& shape, const Shape& hierarchy,
                                            Symbol<cfg::ParallelDistribution> src_nd_sbp,
                                            Symbol<cfg::ParallelDistribution> dst_nd_sbp);

Maybe<Symbol<ParallelDesc>> GetBroadcastSubParallelDesc(
    Symbol<ParallelDesc> parallel_desc, Symbol<cfg::ParallelDistribution> parallel_distribution);

Maybe<std::vector<NaiveBoxingTransformation>> DecomposeByParallelId(
    Symbol<one::ConsistentTensorMeta> tensor_meta, Symbol<cfg::ParallelDistribution> dst_nd_sbp,
    int64_t parallel_id);

Maybe<bool> IsNdSbpBoxingAcyclic(Symbol<cfg::ParallelDistribution> src_nd_sbp,
                                 Symbol<cfg::ParallelDistribution> dst_nd_sbp);

Maybe<std::vector<int64_t>> GetNdSbpValidTransformationAxisSequence(
    Symbol<cfg::ParallelDistribution> src_nd_sbp, Symbol<cfg::ParallelDistribution> dst_nd_sbp);

}  // namespace private_details

static constexpr auto* GetBroadcastSubParallelDesc =
    DECORATE(&private_details::GetBroadcastSubParallelDesc, ThreadLocal);

Maybe<std::vector<NaiveBoxingTransformation>> DecomposeIntoNaiveTransformations(
    Symbol<one::ConsistentTensorMeta> tensor_meta, Symbol<cfg::ParallelDistribution> dst_nd_sbp);

Maybe<std::unordered_map<int64_t, Symbol<ParallelDesc>>> GetBroadcastGroup(
    Symbol<ParallelDesc> src_parallel_desc, Symbol<ParallelDesc> dst_parallel_desc);

Maybe<std::unordered_map<int64_t, Symbol<ParallelDesc>>> GetBroadcastGroupWithoutAcrossNode(
    Symbol<ParallelDesc> src_parallel_desc, Symbol<ParallelDesc> dst_parallel_desc);

}  // namespace oneflow

#endif  // ONEFLOW_CORE_FRAMEWORK_PLACEMENT_SBP_UTIL_H_
