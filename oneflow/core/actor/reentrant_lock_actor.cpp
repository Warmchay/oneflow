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
#include "oneflow/core/actor/actor.h"
#include "oneflow/core/kernel/reentrant_lock_kernel.h"

namespace oneflow {

class ReentrantLockActor final : public Actor {
 public:
  OF_DISALLOW_COPY_AND_MOVE(ReentrantLockActor);
  ReentrantLockActor() : reentrant_lock_status_(nullptr) {}
  ~ReentrantLockActor() override = default;

 protected:
  void VirtualActorInit(const TaskProto&) override;

 private:
  void Act() override;
  void NormalProcessCustomizedReadableRegstMsg(const ActorMsg&) override;
  void ForEachCurCustomizedReadableRegst(std::function<void(const Regst*)>) const override;
  bool IsCustomizedReadReady() const override;
  void NormalProcessCustomizedEordMsg(const ActorMsg&) override {}
  bool IsCustomizedReadAlwaysUnReadyFromNow() const override;
  void AsyncReturnAllCustomizedReadableRegst() override;
  std::pair<RegstNameType, HashSet<std::string>> GetNaiveOrCustomizedConsumedRegstDescName()
      override {
    return std::make_pair(RegstNameType::kNaive, HashSet<std::string>{});
  }
  void VirtualAsyncSendNaiveProducedRegstMsgToConsumer() override;
  void AsyncSendCustomizedConsumedRegstMsgToProducer() override;
  int64_t GetCurProcessedRegstDescId() const;

  const std::string& Ibn4RegstDescId(int64_t id) const;

  RegstSlot consumed_rs_;
  int64_t cur_processed_regst_desc_id_{};
  HashMap<int64_t, std::string> regst_desc_id2ibn_;
  ReentrantLockStatus* reentrant_lock_status_;
  int64_t eord_regst_desc_id_{};
  int64_t act_id_{};
};

void ReentrantLockActor::VirtualActorInit(const TaskProto& task_proto) {
  CHECK_EQ(1, exec_kernel_vec().size());
  reentrant_lock_status_ =
      static_cast<ReentrantLockStatus*>(exec_kernel_vec().at(0).kernel_ctx->state());
  act_id_ = 0;
  const auto& kernel_conf = task_proto.exec_sequence().exec_node().Get(0).kernel_conf();
  const auto& ibns = kernel_conf.op_attribute().input_bns();
  for (const auto& ibn : ibns) {
    int64_t regst_desc_id = exec_kernel_vec().at(0).bn_in_op2blob_info.at(ibn).regst_desc_id;
    if (ibn == "start") { eord_regst_desc_id_ = regst_desc_id; }
    CHECK(regst_desc_id2ibn_.emplace(regst_desc_id, ibn).second);
  }
  for (const auto& pair : task_proto.consumed_regst_desc_id()) {
    for (const int64_t regst_desc_id : pair.second.regst_desc_id()) {
      consumed_rs_.InsertRegstDescId(regst_desc_id);
    }
  }
  consumed_rs_.InitedDone();
  cur_processed_regst_desc_id_ = -1;
  reentrant_lock_status_->Init(kernel_conf);
  OF_SET_MSG_HANDLER(&ReentrantLockActor::HandlerNormal);
}

void ReentrantLockActor::NormalProcessCustomizedReadableRegstMsg(const ActorMsg& msg) {
  CHECK_EQ(0, consumed_rs_.TryPushBackRegst(msg.regst()));
}

bool ReentrantLockActor::IsCustomizedReadReady() const {
  return reentrant_lock_status_->cur_unlocked_ids().size() > 0
         || -1 != GetCurProcessedRegstDescId();
}

void ReentrantLockActor::ForEachCurCustomizedReadableRegst(
    std::function<void(const Regst*)> handler) const {
  handler(consumed_rs_.Front(cur_processed_regst_desc_id_));
}

const std::string& ReentrantLockActor::Ibn4RegstDescId(int64_t id) const {
  const auto& iter = regst_desc_id2ibn_.find(id);
  if (iter == regst_desc_id2ibn_.end()) { return ReentrantLockStatus::kEmptyIbn; }
  return regst_desc_id2ibn_.at(id);
}

void ReentrantLockActor::Act() {
  cur_processed_regst_desc_id_ = GetCurProcessedRegstDescId();
  Regst* const cur_regst = consumed_rs_.Front(cur_processed_regst_desc_id_);
  reentrant_lock_status_->set_cur_ibn(Ibn4RegstDescId(cur_processed_regst_desc_id_));
  reentrant_lock_status_->set_cur_act_id(act_id_);
  act_id_ += 1;
  AsyncLaunchKernel([&](int64_t regst_desc_id) -> Regst* {
    if (cur_processed_regst_desc_id_ != regst_desc_id) { return nullptr; }
    return cur_regst;
  });
}

bool ReentrantLockActor::IsCustomizedReadAlwaysUnReadyFromNow() const {
  return ReceiveEordMsg(eord_regst_desc_id_)
         && reentrant_lock_status_->total_queued_request_lock_num() == 0
         && reentrant_lock_status_->total_acquired_lock_num() == 0;
}

void ReentrantLockActor::VirtualAsyncSendNaiveProducedRegstMsgToConsumer() {
  if (reentrant_lock_status_->acquired_lock_to_be_sent() == false) { return; }
  HandleProducedNaiveDataRegstToConsumer();
}

void ReentrantLockActor::AsyncSendCustomizedConsumedRegstMsgToProducer() {
  Regst* const cur_regst = consumed_rs_.Front(cur_processed_regst_desc_id_);
  if (cur_regst == nullptr) { return; }
  AsyncSendRegstMsgToProducer(cur_regst);
  CHECK_EQ(0, consumed_rs_.TryPopFrontRegst(cur_processed_regst_desc_id_));
  cur_processed_regst_desc_id_ = -1;
}

void ReentrantLockActor::AsyncReturnAllCustomizedReadableRegst() {
  CHECK_EQ(-1, cur_processed_regst_desc_id_);
  CHECK_EQ(0, consumed_rs_.available_regst_desc_cnt());
}

int64_t ReentrantLockActor::GetCurProcessedRegstDescId() const {
  int64_t cur_processed_regst_desc_id = -1;
  consumed_rs_.ForChosenRegstDeq(
      [&cur_processed_regst_desc_id](int64_t) { return cur_processed_regst_desc_id == -1; },
      [&cur_processed_regst_desc_id](const std::deque<Regst*>& reg_deq) {
        if (reg_deq.empty()) { return; }
        cur_processed_regst_desc_id = reg_deq.front()->regst_desc_id();
      });
  return cur_processed_regst_desc_id;
}

REGISTER_ACTOR(kReentrantLock, ReentrantLockActor);

}  // namespace oneflow
