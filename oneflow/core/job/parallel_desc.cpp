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
#include "oneflow/core/job/parallel_desc.h"
#include "oneflow/core/job/placement.cfg.h"
#include "oneflow/core/common/decorator.h"
#include "oneflow/core/common/util.h"
#include "oneflow/core/job/global_for.h"
#include "oneflow/core/job/id_manager.h"
#include "oneflow/core/control/global_process_ctx.h"
#include "oneflow/core/framework/parallel_conf_util.h"
#include "oneflow/core/framework/instructions_builder.h"
#include "oneflow/core/framework/device.h"
#include "oneflow/core/vm/vm_util.h"

namespace oneflow {

namespace {

using MachineId2DeviceIdList =
    std::shared_ptr<HashMap<int64_t, std::shared_ptr<std::vector<int64_t>>>>;

bool GlobalDeviceIdsContaining(const MachineId2DeviceIdList& bigger,
                               const MachineId2DeviceIdList& smaller) {
  for (const auto& pair : *smaller) {
    if (bigger->find(pair.first) == bigger->end()) { return false; }
    const auto& bigger_device_ids = bigger->find(pair.first)->second;
    std::vector<int64_t>::iterator ret;
    for (int64_t device_id : *pair.second) {
      ret = std::find(bigger_device_ids->begin(), bigger_device_ids->end(), device_id);
      if (ret == bigger_device_ids->end()) { return false; }
    }
  }
  return true;
}

}  // namespace

Maybe<void> ParseDeviceNameConf(const std::string& device_name, int64_t* mchn_id,
                                std::string* device_id_str) {
  size_t delimiter_pos = device_name.rfind(":");
  CHECK_NE_OR_RETURN(delimiter_pos, std::string::npos);
  *mchn_id = oneflow_cast<int64_t>(device_name.substr(0, delimiter_pos));
  *device_id_str = device_name.substr(delimiter_pos + 1);
  return Maybe<void>::Ok();
}

Maybe<OFRecord> ParseMachineAndDeviceIdList(const ParallelConf& parallel_conf) {
  ParallelDesc parallel_desc;
  JUST(parallel_desc.MaybeInit(parallel_conf));
  auto machine2device_list = std::make_shared<OFRecord>();
  auto* features = machine2device_list->mutable_feature();
  for (int64_t machine_id : parallel_desc.sorted_machine_ids()) {
    Int32List* device_id_list = (*features)[std::to_string(machine_id)].mutable_int32_list();
    for (int64_t device_id : parallel_desc.sorted_dev_phy_ids(machine_id)) {
      device_id_list->add_value(device_id);
    }
  }
  return machine2device_list;
}

ParallelDesc::ParallelDesc(const ParallelConf& user_conf)
    : symbol_id_(Error::SymbolIdUninitializedError()) {
  CHECK_JUST(MaybeInit(user_conf));
  CHECK_JUST(CheckWithResourceDesc(*(Global<ResourceDesc, ForSession>::Get())));
}

Maybe<ParallelDesc> ParallelDesc::New(int64_t symbol_id, const ParallelConf& parallel_conf) {
  std::shared_ptr<ParallelDesc> parallel_desc(new ParallelDesc(symbol_id));
  JUST(parallel_desc->MaybeInit(parallel_conf));
  return parallel_desc;
}

Maybe<ParallelDesc> ParallelDesc::New(const std::string& device_tag,
                                      const std::vector<std::string>& machine_device_ids,
                                      const std::shared_ptr<Shape>& hierarchy) {
  const auto parallel_conf = JUST(MakeParallelConf(device_tag, machine_device_ids, hierarchy));
  std::shared_ptr<ParallelDesc> parallel_desc;
  JUST(LogicalRun([&parallel_desc, &parallel_conf](InstructionsBuilder* builder) -> Maybe<void> {
    parallel_desc = JUST(builder->GetParallelDescSymbol(parallel_conf));
    return Maybe<void>::Ok();
  }));
  return parallel_desc;
}

Maybe<void> ParallelDesc::MaybeInit(const ParallelConf& user_conf) {
  parallel_conf_ = user_conf;
  device_type_ = DeviceType::kInvalidDevice;
  const std::string& device_tag = parallel_conf_.device_tag();
  DeviceType device_type = JUST(DeviceType4DeviceTag(device_tag));
  CHECK_OR_RETURN(device_type_ == DeviceType::kInvalidDevice || device_type_ == device_type);
  device_type_ = device_type;
  machine_id2sorted_dev_phy_ids_ =
      std::make_shared<HashMap<int64_t, std::shared_ptr<std::vector<int64_t>>>>();
  for (const std::string& device_name : parallel_conf_.device_name()) {
    if (device_name[0] == '@') {
      JUST(SetMachineIdAndDeviceIdsByParsingDeviceName(device_name.substr(1), 1));
    } else {
      JUST(SetMachineIdAndDeviceIdsByParsingDeviceName(device_name,
                                                       GlobalProcessCtx::NumOfProcessPerNode()));
    }
  }
  containing_current_rank_ = machine_id2sorted_dev_phy_ids_->count(GlobalProcessCtx::Rank()) > 0;
  ClearUp();
  JUST(SanityCheck());
  return Maybe<void>::Ok();
}

Maybe<void> ParallelDesc::SetMachineIdAndDeviceIdsByParsingDeviceName(
    const std::string& device_name, size_t cols) {
  int64_t node_id = -1;
  std::string device_id_str;
  JUST(ParseDeviceNameConf(device_name, &node_id, &device_id_str));
  int64_t minus_pos = device_id_str.find("-");
  if (minus_pos == std::string::npos) {
    device_id_str = device_id_str + "-" + device_id_str;
    minus_pos = device_id_str.find("-");
  }
  int64_t min_id = oneflow_cast<int64_t>(device_id_str.substr(0, minus_pos));
  int64_t max_id = oneflow_cast<int64_t>(device_id_str.substr(minus_pos + 1));
  CHECK_LE_OR_RETURN(min_id, max_id);
  for (int64_t dev_phy_id = min_id; dev_phy_id <= max_id; ++dev_phy_id) {
    int64_t mchn_id = dev_phy_id % cols + node_id * cols;
    if (!(*machine_id2sorted_dev_phy_ids_)[mchn_id]) {
      (*machine_id2sorted_dev_phy_ids_)[mchn_id] = std::make_shared<std::vector<int64_t>>();
    }
    (*machine_id2sorted_dev_phy_ids_)[mchn_id]->push_back(dev_phy_id);
  }
  return Maybe<void>::Ok();
}

Maybe<int64_t> ParallelDesc::ParallelId4MachineDeviceId(int64_t machine_id,
                                                        int64_t device_id) const {
  const auto& machine_iter = machine_id2device_id2parallel_id_.find(machine_id);
  CHECK_OR_RETURN(machine_iter != machine_id2device_id2parallel_id_.end());
  const auto& device_iter = machine_iter->second.find(device_id);
  CHECK_OR_RETURN(device_iter != machine_iter->second.end());
  return device_iter->second;
}

Maybe<Symbol<Device>> ParallelDesc::GetTensorDevice4CurrentProcessCtx(
    Optional<int64_t>* parallel_id) const {
  int64_t machine_id = 0;
  int64_t device_id = 0;
  GlobalProcessCtx::GetCurrentMachineIdAndDeviceId(&machine_id, &device_id);
  const auto& device =
      JUST(Device::ThreadLocalGetOrNew(Device::Type4DeviceTag(device_tag()), device_id));
  int64_t parallel_id_val = -1;
  if (TryGetParallelId(machine_id, device_id, &parallel_id_val)) {
    *parallel_id = parallel_id_val;
  } else {
    *parallel_id = Optional<int64_t>();
  }
  return device;
}

Maybe<Symbol<Device>> GetTensorDevice4CurrentProcessCtx(Symbol<ParallelDesc> parallel_desc,
                                                        Optional<int64_t>* parallel_id) {
  static thread_local HashMap<Symbol<ParallelDesc>, Optional<int64_t>> parallel_desc2parallel_id;
  static thread_local HashMap<Symbol<ParallelDesc>, Symbol<Device>> parallel_desc2device;
  auto parallel_id_iter = parallel_desc2parallel_id.find(parallel_desc);
  auto device_iter = parallel_desc2device.find(parallel_desc);
  if (device_iter == parallel_desc2device.end()) {
    CHECK_OR_RETURN(parallel_id_iter == parallel_desc2parallel_id.end());
    Optional<int64_t> id_val;
    const auto& device_symbol = JUST(parallel_desc->GetTensorDevice4CurrentProcessCtx(&id_val));
    parallel_id_iter = parallel_desc2parallel_id.emplace(parallel_desc, id_val).first;
    device_iter = parallel_desc2device.emplace(parallel_desc, device_symbol).first;
  } else {
    CHECK_OR_RETURN(parallel_id_iter != parallel_desc2parallel_id.end());
  }
  *parallel_id = parallel_id_iter->second;
  return device_iter->second;
}

bool ParallelDesc::TryGetParallelId(int64_t machine_id, int64_t device_id,
                                    int64_t* parallel_id) const {
  const auto& machine_iter = machine_id2device_id2parallel_id_.find(machine_id);
  if (machine_iter == machine_id2device_id2parallel_id_.end()) { return false; }
  const auto& device_iter = machine_iter->second.find(device_id);
  if (device_iter == machine_iter->second.end()) { return false; }
  *parallel_id = device_iter->second;
  return true;
}

Maybe<void> ParallelDesc::GetParallelContext(ParallelContext* parallel_ctx, int64_t machine_id,
                                             int64_t device_id) const {
  parallel_ctx->set_parallel_num(parallel_num());
  parallel_ctx->set_parallel_id(JUST(ParallelId4MachineDeviceId(machine_id, device_id)));
  return Maybe<void>::Ok();
}

bool ParallelDesc::Equals(const ParallelDesc& rhs) const {
  return (this == &rhs)
         || (device_type_ == rhs.device_type_ && sorted_machine_ids_ == rhs.sorted_machine_ids_
             && EqualsMachineId2SortedDevPhyIds(rhs) && *hierarchy_ == *rhs.hierarchy_);
}

bool ParallelDesc::EqualsIgnoringDeviceType(const ParallelDesc& rhs) const {
  return sorted_machine_ids_ == rhs.sorted_machine_ids_ && EqualsMachineId2SortedDevPhyIds(rhs)
         && *hierarchy_ == *rhs.hierarchy_;
}

bool ParallelDesc::EqualsIgnoringHierarchy(const ParallelDesc& rhs) const {
  return (this == &rhs)
         || (device_type_ == rhs.device_type_ && sorted_machine_ids_ == rhs.sorted_machine_ids_
             && EqualsMachineId2SortedDevPhyIds(rhs));
}

bool ParallelDesc::EqualsOnlyForMachineAndDeviceIds(const ParallelDesc& rhs) const {
  return (this == &rhs)
         || (sorted_machine_ids_ == rhs.sorted_machine_ids_
             && EqualsMachineId2SortedDevPhyIds(rhs));
}

bool ParallelDesc::EqualsMachineId2SortedDevPhyIds(const ParallelDesc& rhs) const {
  for (int64_t machine_id : sorted_machine_ids_) {
    if (*machine_id2sorted_dev_phy_ids_->at(machine_id)
        != *rhs.machine_id2sorted_dev_phy_ids_->at(machine_id)) {
      return false;
    }
  }
  return true;
}

void ParallelDesc::ClearUp() {
  EraseIf<int64_t, std::shared_ptr<std::vector<int64_t>>>(
      machine_id2sorted_dev_phy_ids_.get(),
      [](HashMap<int64_t, std::shared_ptr<std::vector<int64_t>>>::iterator it) {
        return it->second->empty();
      });
  sorted_machine_ids_.clear();
  parallel_num_ = 0;
  for (auto& pair : *machine_id2sorted_dev_phy_ids_) {
    sorted_machine_ids_.push_back(pair.first);
    SortAndRemoveDuplication((pair.second).get());
    parallel_num_ += pair.second->size();
  }
  if (parallel_conf_.has_hierarchy() && parallel_conf_.hierarchy().dim_size() != 0) {
    hierarchy_.reset(new Shape(parallel_conf_.hierarchy()));
    CHECK_EQ(hierarchy_->elem_cnt(), parallel_num_);
  } else {
    hierarchy_.reset(new Shape({parallel_num_}));
    hierarchy_->ToProto(parallel_conf_.mutable_hierarchy());
  }
  SortAndRemoveDuplication(&sorted_machine_ids_);
  parallel_conf_.clear_device_name();
  int64_t parallel_id = 0;
  for (int64_t machine_id : sorted_machine_ids_) {
    for (int64_t device_id : *machine_id2sorted_dev_phy_ids_->at(machine_id)) {
      parallel_conf_.add_device_name(std::string("@") + std::to_string(machine_id) + ":"
                                     + std::to_string(device_id));
      parallel_id2machine_id_[parallel_id] = machine_id;
      parallel_id2device_id_[parallel_id] = device_id;
      machine_id2device_id2parallel_id_[machine_id][device_id] = parallel_id;
      parallel_id += 1;
    }
  }
  cfg_parallel_conf_.reset(new cfg::ParallelConf(parallel_conf_));
}

void ParallelDesc::set_device_type(DeviceType device_type) {
  if (device_type == device_type_) { return; }
  device_type_ = device_type;
  const std::string tag = *CHECK_JUST(DeviceTag4DeviceType(device_type));
  parallel_conf_.set_device_tag(tag);
}

Maybe<void> ParallelDesc::SanityCheck() {
  device_num_of_each_machine_ = -1;
  for (auto& pair : *machine_id2sorted_dev_phy_ids_) {
    if (device_num_of_each_machine_ == -1) {
      device_num_of_each_machine_ = pair.second->size();
    } else {
      CHECK_EQ_OR_RETURN(device_num_of_each_machine_, pair.second->size());
    }
  }
  return Maybe<void>::Ok();
}

Maybe<void> ParallelDesc::CheckWithResourceDesc(const ResourceDesc& resource_desc) {
  if (device_type_ == DeviceType::kGPU) {
    for (auto& pair : *machine_id2sorted_dev_phy_ids_) {
      for (int64_t dev_phy_id : *pair.second) {
        CHECK_LT_OR_RETURN(dev_phy_id, resource_desc.GpuDeviceNum());
      }
    }
  }
  return Maybe<void>::Ok();
}

ParallelConf ParallelDesc::GetParallelIdOnlyParallelConf(int64_t parallel_id) const {
  ParallelConf parallel_conf;
  std::string rank = std::to_string(CHECK_JUST(MachineId4ParallelId(parallel_id)));
  std::string device_id = std::to_string(CHECK_JUST(DeviceId4ParallelId(parallel_id)));
  parallel_conf.set_device_tag(*CHECK_JUST(DeviceTag4DeviceType(device_type())));
  parallel_conf.add_device_name(std::string("@") + rank + ":" + device_id);
  return parallel_conf;
}

Maybe<int64_t> ParallelDesc::MachineId4ParallelId(int64_t parallel_id) const {
  const auto& iter = parallel_id2machine_id_.find(parallel_id);
  CHECK_OR_RETURN(iter != parallel_id2machine_id_.end())
      << "parallel_id: " << parallel_id << "\n----[ parallel_conf ]----"
      << parallel_conf().DebugString();
  return iter->second;
}

Maybe<int64_t> ParallelDesc::DeviceId4ParallelId(int64_t parallel_id) const {
  const auto& iter = parallel_id2device_id_.find(parallel_id);
  CHECK_OR_RETURN(iter != parallel_id2device_id_.end())
      << "parallel_id: " << parallel_id << "\n----[ parallel_conf ]----"
      << parallel_conf().DebugString();
  return iter->second;
}

bool ParallelDesc::ContainingMachineId(int64_t machine_id) const {
  return machine_id2sorted_dev_phy_ids_->find(machine_id) != machine_id2sorted_dev_phy_ids_->end();
}

bool ParallelDesc::Containing(int64_t machine_id, int64_t device_id) const {
  const auto& machine_iter = machine_id2sorted_dev_phy_ids_->find(machine_id);
  if (machine_iter == machine_id2sorted_dev_phy_ids_->end()) { return false; }
  const auto& vec = machine_iter->second;
  return std::find(vec->begin(), vec->end(), device_id) != vec->end();
}

bool ParallelDesc::Bigger(const ParallelDesc& rhs) const {
  if (device_tag() != rhs.device_tag()) { return false; }
  return GlobalDeviceIdsContaining(machine_id2sorted_dev_phy_ids_,
                                   rhs.machine_id2sorted_dev_phy_ids());
}

std::tuple<int32_t, int32_t> GetPartIdAndPartNumFromParallelCtx(
    const ParallelContext* parallel_ctx) {
  return std::make_tuple(parallel_ctx->parallel_id(), parallel_ctx->parallel_num());
}

ParallelConf GenParallelConfOfCpuZeroOnMaster() {
  ParallelConf parallel_conf;
  parallel_conf.set_device_tag("cpu");
  parallel_conf.add_device_name("0:0");
  return parallel_conf;
}

ParallelConf GenParallelConfOfCpuZeroOnAllMachines() {
  ParallelConf parallel_conf;
  parallel_conf.set_device_tag("cpu");
  for (int64_t i : Global<ResourceDesc, ForSession>::Get()->process_ranks()) {
    parallel_conf.add_device_name(std::string("@") + std::to_string(i) + ":0");
  }
  return parallel_conf;
}

bool IsMirroredParallelContext(const ParallelContext& parallel_ctx) {
  if (CHECK_JUST(GlobalMultiClientEnv())) {
    return parallel_ctx.parallel_id() == 0 && parallel_ctx.parallel_num() == 1
           && GlobalProcessCtx::WorldSize() > 1;
  }
  return false;
}

namespace {

Maybe<Optional<int64_t>> CalcParallelId4CurrentProcessCtx(Symbol<ParallelDesc> parallel_desc) {
  int64_t machine_id = 0;
  int64_t device_id = 0;
  GlobalProcessCtx::GetCurrentMachineIdAndDeviceId(&machine_id, &device_id);
  int64_t parallel_id = -1;
  if (parallel_desc->TryGetParallelId(machine_id, device_id, &parallel_id)) {
    return Optional<int64_t>(parallel_id);
  } else {
    return Optional<int64_t>();
  }
}

Maybe<const ParallelContext> CalcParallelContext4CurrentProcessCtx(
    Symbol<ParallelDesc> parallel_desc) {
  int64_t machine_id = 0;
  int64_t device_id = 0;
  GlobalProcessCtx::GetCurrentMachineIdAndDeviceId(&machine_id, &device_id);
  int64_t parallel_id_val = -1;
  CHECK_OR_RETURN(parallel_desc->TryGetParallelId(machine_id, device_id, &parallel_id_val));
  std::shared_ptr<ParallelContext> parallel_ctx = std::make_shared<ParallelContext>();
  parallel_ctx->set_parallel_id(parallel_id_val);
  parallel_ctx->set_parallel_num(parallel_desc->parallel_num());
  return std::shared_ptr<const ParallelContext>(parallel_ctx);
}

Maybe<Symbol<ParallelDesc>> RawReplaceDeviceType(Symbol<ParallelDesc> parallel_desc,
                                                 DeviceType device_type) {
  ParallelConf parallel_conf(parallel_desc->parallel_conf());
  parallel_conf.set_device_tag(*JUST(DeviceTag4DeviceType(device_type)));
  return SymbolOf(ParallelDesc(parallel_conf));
}

Maybe<std::string> RawPlacementToString(Symbol<ParallelDesc> placement) {
  std::string device_type = placement->device_tag() == "gpu" ? "\"cuda\"" : "\"cpu\"";
  std::vector<int64_t> sorted_node_ids;
  HashMap<int64_t, std::vector<int64_t>> node_id2sorted_dev_phy_ids;
  for (int64_t machine_id : placement->sorted_machine_ids()) {
    int64_t node_id = GlobalProcessCtx::NodeId(machine_id);
    if (!std::count(sorted_node_ids.begin(), sorted_node_ids.end(), node_id)) {
      sorted_node_ids.push_back(node_id);
    }
    for (int64_t device_id : placement->sorted_dev_phy_ids(machine_id)) {
      node_id2sorted_dev_phy_ids[node_id].push_back(device_id);
    }
  }
  std::string machine_device_ids = "{";
  int64_t node_idx = 0;
  for (int64_t node_id : sorted_node_ids) {
    std::string device_name = std::to_string(node_id) + " : [";
    int64_t device_idx = 0;
    for (int64_t device_id : node_id2sorted_dev_phy_ids.at(node_id)) {
      device_name += std::to_string(device_id);
      if (++device_idx != node_id2sorted_dev_phy_ids.at(node_id).size()) { device_name += ", "; }
    }
    device_name += "]";
    if (++node_idx != sorted_node_ids.size()) { device_name += ", "; }
    machine_device_ids += device_name;
  }
  machine_device_ids += "}";
  std::string hierarchy = "(";
  int32_t hierarchy_dim_idx = 0;
  for (int64_t dim : placement->hierarchy()->dim_vec()) {
    hierarchy += std::to_string(dim);
    if (++hierarchy_dim_idx != placement->hierarchy()->dim_vec().size()) {
      hierarchy += ", ";
    } else if (placement->hierarchy()->dim_vec().size() == 1) {
      hierarchy += ",";
    }
  }
  hierarchy += ")";
  std::string placement_str = "oneflow.placement(device_type=" + device_type
                              + ", machine_device_ids=" + machine_device_ids
                              + ", hierarchy=" + hierarchy + ")";
  return placement_str;
}

Maybe<Symbol<Device>> RawGetTensorDevice(Symbol<ParallelDesc> parallel_desc) {
  int64_t machine_id = 0;
  int64_t device_id = 0;
  GlobalProcessCtx::GetCurrentMachineIdAndDeviceId(&machine_id, &device_id);
  const auto& type = Device::Type4DeviceTag(parallel_desc->device_tag());
  return JUST(Device::ThreadLocalGetOrNew(type, device_id));
}

}  // namespace

decltype(GetParallelId4CurrentProcessCtx) GetParallelId4CurrentProcessCtx =
    DECORATE(&CalcParallelId4CurrentProcessCtx, ThreadLocal);
decltype(GetParallelContext4CurrentProcessCtx) GetParallelContext4CurrentProcessCtx =
    DECORATE(&CalcParallelContext4CurrentProcessCtx, ThreadLocal);
decltype(ReplaceDeviceType) ReplaceDeviceType = DECORATE(&RawReplaceDeviceType, ThreadLocal);
decltype(PlacementToString) PlacementToString = DECORATE(&RawPlacementToString, ThreadLocal);
decltype(GetTensorDevice) GetTensorDevice = DECORATE(&RawGetTensorDevice, ThreadLocal);

}  // namespace oneflow
