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
#include <sstream>
#include "oneflow/core/common/error_util.h"
#include "oneflow/core/common/util.h"

namespace oneflow {

namespace {

std::string StripSpace(std::string str) {
  if (str.size() == 0) { return ""; }
  size_t pos = str.find_first_not_of(" ");
  if (pos != std::string::npos) { str.erase(0, pos); }
  pos = str.find_last_not_of(" ");
  if (pos != std::string::npos) { str.erase(pos + 1); }
  return str;
}

bool IsLetterNumberOrUnderline(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

std::string StripBrackets(std::string str) {
  if (str.size() == 0) { return ""; }
  if (str.at(0) != '(') { return str; }
  // "()" come from OF_PP_STRINGIZE((__VA_ARGS__)), so size() >= 2
  str = str.substr(1, str.size() - 2);
  return str;
}

Maybe<std::string> ShortenMsg(std::string str) {
  // 150 characters is the threshold
  const int num_character_threshold = 150;
  const int num_displayed_character = 50;
  if (str.size() == 0) { return str; }
  // strip space when JUST(  xx  );
  str = StripSpace(str);
  if (str.size() < num_character_threshold) { return str; }

  // left part whose number of characters is just over 50
  int left_index = num_displayed_character;
  bool pre_condition = IsLetterNumberOrUnderline(str.at(left_index));
  for (; left_index < str.size(); left_index++) {
    bool cur_condition = IsLetterNumberOrUnderline(str.at(left_index));
    if ((pre_condition && !cur_condition) || (!pre_condition && cur_condition)) { break; }
  }

  // right part whose number of characters is just over 50
  int right_index = str.size() - num_displayed_character;
  pre_condition = IsLetterNumberOrUnderline(str.at(right_index));
  for (; right_index >= 0; right_index--) {
    bool cur_condition = IsLetterNumberOrUnderline(str.at(right_index));
    if ((pre_condition && !cur_condition) || (!pre_condition && cur_condition)) {
      right_index++;
      break;
    }
  }
  // a long word of more than 150
  if (right_index - left_index < 50) { return str; }
  std::stringstream ss;
  CHECK_OR_RETURN(left_index >= 0);
  CHECK_OR_RETURN(left_index < str.size());
  ss << str.substr(0, left_index);
  ss << " ... ";
  CHECK_OR_RETURN(right_index >= 0);
  CHECK_OR_RETURN(right_index < str.size());
  ss << str.substr(right_index);
  return ss.str();
}

// file info in stack frame
std::string FormatFileOfStackFrame(const std::string& file) {
  std::stringstream ss;
  ss << "\n  File \"" << file << "\", ";
  return ss.str();
}

// line info in stack frame
std::string FormatLineOfStackFrame(const int64_t& line) {
  std::stringstream ss;
  ss << "line " << line << ",";
  return ss.str();
}

// function info in stack frame
std::string FormatFunctionOfStackFrame(const std::string& function) {
  std::stringstream ss;
  ss << " in " << function;
  return ss.str();
}

// msg in stack frame
Maybe<std::string> FormatMsgOfStackFrame(std::string error_msg, bool is_last_stack_frame) {
  error_msg = StripBrackets(error_msg);
  if (!is_last_stack_frame) { error_msg = *JUST(ShortenMsg(error_msg)); }
  // error_msg of last stack frame come from "<<"
  if (is_last_stack_frame) { error_msg = StripSpace(error_msg); }
  std::stringstream ss;
  ss << "\n    " << error_msg;
  return ss.str();
}

// the error_summary and msg in error proto
std::string FormatErrorSummaryAndMsgOfErrorProto(const std::shared_ptr<cfg::ErrorProto>& error) {
  std::stringstream ss;
  if (error->has_error_summary()) { ss << error->error_summary(); }
  if (error->has_msg()) { ss << (ss.str().size() != 0 ? "\n" + error->msg() : error->msg()); }
  return ss.str();
}

// the msg in error type instance.
Maybe<std::string> FormatMsgOfErrorType(const std::shared_ptr<cfg::ErrorProto>& error) {
  CHECK_NE_OR_RETURN(error->error_type_case(), cfg::ErrorProto::ERROR_TYPE_NOT_SET);
  std::stringstream ss;
  ErrorProto pb_error;
  error->ToProto(&pb_error);
  const google::protobuf::Descriptor* pb_error_des = pb_error.GetDescriptor();
  const google::protobuf::OneofDescriptor* oneof_field_des =
      pb_error_des->FindOneofByName("error_type");
  const google::protobuf::Reflection* pb_error_ref = pb_error.GetReflection();
  const google::protobuf::FieldDescriptor* field_des =
      pb_error_ref->GetOneofFieldDescriptor(pb_error, oneof_field_des);
  CHECK_OR_RETURN(field_des != nullptr);
  const google::protobuf::Message& error_type = pb_error_ref->GetMessage(pb_error, field_des);
  ss << error_type.DebugString();
  return ss.str();
}

}  // namespace

Maybe<std::string> FormatErrorStr(const std::shared_ptr<cfg::ErrorProto>& error) {
  std::stringstream ss;
  // Get msg from stack frame of error proto
  for (auto stack_frame = error->mutable_stack_frame()->rbegin();
       stack_frame < error->mutable_stack_frame()->rend(); stack_frame++) {
    ss << FormatFileOfStackFrame(*stack_frame->mutable_file())
       << FormatLineOfStackFrame(*stack_frame->mutable_line())
       << FormatFunctionOfStackFrame(*stack_frame->mutable_function())
       << *JUST(FormatMsgOfStackFrame(*stack_frame->mutable_error_msg(),
                                      stack_frame == error->mutable_stack_frame()->rend() - 1));
  }
  // Get msg from error summary and msg of error proto
  std::string error_summary_and_msg_of_error_proto = FormatErrorSummaryAndMsgOfErrorProto(error);
  if (error_summary_and_msg_of_error_proto.size() != 0) {
    ss << "\n" << error_summary_and_msg_of_error_proto;
  }
  // Get msg from error type of error proto
  std::string msg_of_error_type = *JUST(FormatMsgOfErrorType(error));
  if (msg_of_error_type.size() != 0) { ss << "\n" << msg_of_error_type; }
  return ss.str();
}

}  // namespace oneflow
