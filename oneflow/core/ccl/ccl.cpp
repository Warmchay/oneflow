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
#include "oneflow/core/ccl/ccl.h"
#include "oneflow/core/framework/transport_util.h"
#include "oneflow/core/job/parallel_desc.h"
#include "oneflow/core/job/rank_group.h"
#include "oneflow/core/common/data_type.h"
#include "oneflow/core/common/data_type_seq.h"
#include "oneflow/core/common/balanced_splitter.h"
#include "oneflow/core/rpc/include/global_process_ctx.h"
#include "oneflow/core/thread/thread_manager.h"

namespace oneflow {
namespace ccl {

namespace {

Maybe<void> InitBroadcastRankHeap(std::vector<int64_t>* ranks, const ParallelDesc& parallel_desc,
                                  int64_t root) {
  CHECK_EQ_OR_RETURN(parallel_desc.parallel_num(), parallel_desc.sorted_machine_ids().size());
  ranks->resize(parallel_desc.parallel_num());
  int64_t root_index = -1;
  for (int64_t parallel_id = 0; parallel_id < parallel_desc.parallel_num(); ++parallel_id) {
    int64_t machine_id = JUST(parallel_desc.MachineId4ParallelId(parallel_id));
    if (machine_id == root) { root_index = parallel_id; }
    (*ranks)[parallel_id] = machine_id;
  }
  CHECK_NE_OR_RETURN(root_index, -1);
  std::swap((*ranks)[0], (*ranks)[root_index]);
  return Maybe<void>::Ok();
}

int64_t RingDecrease(int64_t n, int64_t size) { return (n - 1 + size) % size; }

int64_t RingIncrease(int64_t n, int64_t size) { return (n + 1 + size) % size; }

template<typename T>
void VecAdd(size_t size, T* out, const T* in0, const T* in1) {
  size_t thread_num = Global<ThreadPool>::Get()->thread_num();
  BalancedSplitter bs(size, thread_num);
  MultiThreadLoop(thread_num, [&](size_t thread_idx) {
    size_t end = bs.At(thread_idx).end();
    for (size_t i = bs.At(thread_idx).begin(); i < end; ++i) { out[i] = in0[i] + in1[i]; }
  });
}

}  // namespace

template<typename T, ReduceType reduce_type>
struct DtypeAllReduce;

template<typename T>
struct DtypeAllReduce<T, kSum> {
  static Maybe<void> Call(const void* void_in, void* void_out, size_t elem_cnt,
                          Symbol<ParallelDesc> parallel_desc) {
    const T* in = reinterpret_cast<const T*>(void_in);
    T* out = reinterpret_cast<T*>(void_out);
    int64_t parallel_num = parallel_desc->parallel_num();
    BalancedSplitter bs(elem_cnt, parallel_num);
    std::vector<T> recv_buffer(bs.At(0).size());
    Optional<int64_t> parallel_id;
    JUST(GetDevice4CurrentProcessCtx(parallel_desc, &parallel_id));
    const auto& rank_group = JUST(RankGroup::New(parallel_desc));
    TransportToken transport_token = TransportToken::NewDataTransportToken();
    for (int64_t i = 0, part_id = JUST(parallel_id.value()); i < parallel_num - 1;
         ++i, part_id = RingDecrease(part_id, parallel_num)) {
      int64_t send_part_id = part_id;
      const T* send_ptr = &(i == 0 ? in : out)[bs.At(send_part_id).begin()];
      size_t send_size = bs.At(send_part_id).size();
      int64_t recv_part_id = RingDecrease(part_id, parallel_num);
      T* recv_ptr = recv_buffer.data();
      size_t recv_size = bs.At(recv_part_id).size();
      NaiveAsyncTransportCtx ctx(
          transport_token,
          [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
            *buffer = const_cast<T*>(send_ptr);
            *size = send_size;
            *Cb = [] {};
            return Maybe<void>::Ok();
          },
          [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
            *buffer = recv_ptr;
            *size = recv_size;
            *Cb = [] {};
            return Maybe<void>::Ok();
          });
      if (send_size > 0) {
        JUST(TransportUtil::SendToNextRankInRing(rank_group, transport_token, &ctx));
      }
      if (recv_size > 0) {
        JUST(TransportUtil::ReceiveFromPrevRankInRing(rank_group, transport_token, &ctx));
      }
      JUST(TransportUtil::WaitUntilDoneOrTimeout(ctx, TransportUtil::TimeoutSeconds()));
      const T* cur_in = &(i == 0 ? in : out)[bs.At(recv_part_id).begin()];
      T* cur_out = &out[bs.At(recv_part_id).begin()];
      if (recv_size > 0) { VecAdd(recv_size, cur_out, cur_in, recv_ptr); }
    }
    for (int64_t i = 0, part_id = RingIncrease(JUST(parallel_id.value()), parallel_num);
         i < parallel_num - 1; ++i, part_id = RingDecrease(part_id, parallel_num)) {
      int64_t send_part_id = part_id;
      const T* send_ptr = &out[bs.At(send_part_id).begin()];
      size_t send_size = bs.At(send_part_id).size();
      int64_t recv_part_id = RingDecrease(part_id, parallel_num);
      T* recv_ptr = &out[bs.At(recv_part_id).begin()];
      size_t recv_size = bs.At(recv_part_id).size();
      NaiveAsyncTransportCtx ctx(
          transport_token,
          [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
            *buffer = const_cast<T*>(send_ptr);
            *size = send_size;
            *Cb = [] {};
            return Maybe<void>::Ok();
          },
          [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
            *buffer = recv_ptr;
            *size = recv_size;
            *Cb = [] {};
            return Maybe<void>::Ok();
          });
      if (send_size > 0) {
        JUST(TransportUtil::SendToNextRankInRing(rank_group, transport_token, &ctx));
      }
      if (recv_size > 0) {
        JUST(TransportUtil::ReceiveFromPrevRankInRing(rank_group, transport_token, &ctx));
      }
      JUST(TransportUtil::WaitUntilDoneOrTimeout(ctx, TransportUtil::TimeoutSeconds()));
    }
    return Maybe<void>::Ok();
  }
};

#define MAKE_ALL_REDUCE_ENTRY(func_name, T, reduce_type) func_name<T, reduce_type>::Call

DEFINE_STATIC_SWITCH_FUNC(Maybe<void>, DtypeAllReduce, MAKE_ALL_REDUCE_ENTRY,
                          MAKE_DATA_TYPE_CTRV_SEQ(POD_DATA_TYPE_SEQ), CCL_REDUCE_TYPE_CTRV_SEQ);

#undef MAKE_ALL_REDUCE_ENTRY

template<>
Maybe<void> AllReduce<DeviceType::kCPU>(const void* in, void* out, size_t elem_cnt, DataType dtype,
                                        ReduceType reduce_type, Symbol<ParallelDesc> parallel_desc,
                                        DeviceCtx* ctx) {
  return SwitchDtypeAllReduce(SwitchCase(dtype, reduce_type), in, out, elem_cnt, parallel_desc);
}

template<>
Maybe<void> Broadcast<DeviceType::kCPU>(const void* in, void* out, size_t elem_cnt, DataType dtype,
                                        int64_t root, Symbol<ParallelDesc> parallel_desc,
                                        DeviceCtx* ctx) {
  CHECK_EQ_OR_RETURN(parallel_desc->device_type(), DeviceType::kCPU);
  static thread_local std::vector<int64_t> rank_heap{};
  JUST(InitBroadcastRankHeap(&rank_heap, *parallel_desc, root));
  TransportToken transport_token = TransportToken::NewDataTransportToken();
  CHECK_OR_RETURN(IsPODDataType(dtype));
  size_t buffer_size = elem_cnt * GetSizeOfDataType(dtype);
  NaiveAsyncTransportCtx transport_ctx(
      transport_token,
      [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
        *buffer = (root == GlobalProcessCtx::Rank() ? const_cast<void*>(in) : out);
        *size = buffer_size;
        *Cb = [] {};
        return Maybe<void>::Ok();
      },
      [&](void** buffer, std::size_t* size, std::function<void()>* Cb) -> Maybe<void> {
        *buffer = out;
        *size = buffer_size;
        *Cb = [] {};
        return Maybe<void>::Ok();
      });
  JUST(TransportUtil::ReceiveDataFromParentInHeap(rank_heap, transport_token, &transport_ctx));
  JUST(TransportUtil::WaitUntilDoneOrTimeout(transport_ctx, TransportUtil::TimeoutSeconds()));
  JUST(TransportUtil::SendDataToChildrenInHeap(rank_heap, transport_token, &transport_ctx));
  if (GlobalProcessCtx::Rank() == root && out != in) { std::memcpy(out, in, buffer_size); }
  JUST(TransportUtil::WaitUntilDoneOrTimeout(transport_ctx, TransportUtil::TimeoutSeconds()));
  return Maybe<void>::Ok();
}

}  // namespace ccl
}  // namespace oneflow
