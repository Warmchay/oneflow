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
#ifndef ONEFLOW_CORE_THREAD_THREAD_H_
#define ONEFLOW_CORE_THREAD_THREAD_H_

#include "oneflow/core/actor/actor_message_bus.h"
#include "oneflow/core/common/channel.h"
#include "oneflow/core/common/util.h"
#include "oneflow/core/job/task.pb.h"
#include "oneflow/core/thread/thread_context.h"
#include "oneflow/core/actor/actor.h"

namespace oneflow {

class Thread {
 public:
  OF_DISALLOW_COPY_AND_MOVE(Thread);
  virtual ~Thread();

  void AddTask(const TaskProto&);

  Channel<ActorMsg>* GetMsgChannelPtr() { return &msg_channel_; }

  inline void EnqueueActorMsg(const ActorMsg& msg) {
    if (UseLocalMsgQueue()) {
      local_msg_queue_.push(msg);
    } else {
      msg_channel_.Send(msg);
    }
  }

  template<typename InputIt>
  inline void EnqueueActorMsg(InputIt first, InputIt last) {
    if (UseLocalMsgQueue()) {
      for (auto it = first; it != last; ++it) { local_msg_queue_.push(*it); }
    } else {
      for (auto it = first; it != last; ++it) { msg_channel_.Send(*it); }
    }
  }

  void JoinAllActor() { actor_thread_.join(); }

 protected:
  Thread();
  std::thread& mut_actor_thread() { return actor_thread_; }
  void PollMsgChannel(const ThreadCtx& thread_ctx);
  void set_thrd_id(int64_t val) { thrd_id_ = val; }

 private:
  void ConstructActor(int64_t actor_id, const ThreadCtx& thread_ctx);

  inline bool UseLocalMsgQueue() const {
    return local_msg_queue_enabled_ && std::this_thread::get_id() == actor_thread_.get_id();
  }

  HashMap<int64_t, TaskProto> id2task_;
  std::mutex id2task_mtx_;

  std::thread actor_thread_;
  Channel<ActorMsg> msg_channel_;
  HashMap<int64_t, std::unique_ptr<ActorBase>> id2actor_ptr_;
  HashMap<int64_t, int64_t> id2job_id_;
  std::queue<ActorMsg> local_msg_queue_;
  bool local_msg_queue_enabled_;
  int64_t thrd_id_;
  bool light_actor_enabled_;
};

}  // namespace oneflow

#endif  // ONEFLOW_CORE_THREAD_THREAD_H_
