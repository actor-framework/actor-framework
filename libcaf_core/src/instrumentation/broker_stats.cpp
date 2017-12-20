/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/instrumentation/broker_stats.hpp"

namespace caf {
namespace instrumentation {

const std::unordered_map<msgtype_id, stat_stream>& broker_stats::get_forward_wait_durations() const {
  return forward_waittimes_;
}
const stat_stream& broker_stats::get_forward_size() const {
  return forward_mb_size_;
}
const std::unordered_map<msgtype_id, size_t>& broker_stats::get_message_counts() const {
  return receive_msg_count_;
}

std::string broker_stats::to_string() const {
  std::string res;
  res.reserve(4096);
  for (const auto& fwt : get_forward_wait_durations()) {
    res += "BROKER | FORWARD WAIT TIME | ";
    res += "MSGTYPE: " + caf::instrumentation::to_string(fwt.first);
    res += " => ";
    res += fwt.second.to_string();
    res += "\n";
  }
  if (!get_forward_size().empty())
  {
    res += "BROKER | FORWARD MAILBOX SIZE | ";
    res += get_forward_size().to_string();
    res += "\n";
  }
  for (const auto& rmc : get_message_counts()) {
    res += "BROKER | RECEIVE COUNT | ";
    res += "MSGTYPE: " + caf::instrumentation::to_string(rmc.first);
    res += " => ";
    res += std::to_string(rmc.second);
    res += "\n";
  }
  return res;
}

void lockable_broker_stats::record_broker_receive(msgtype_id mt) {
  std::lock_guard<std::mutex> g(access_mutex_);
  ++receive_msg_count_[mt];
}

void lockable_broker_stats::record_broker_forward(msgtype_id mt, int64_t mb_waittime, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  forward_waittimes_[mt].record(mb_waittime);
  forward_mb_size_.record(mb_size);
}

broker_stats lockable_broker_stats::collect() {
  broker_stats zero;
  std::lock_guard<std::mutex> g(access_mutex_);
  std::swap(forward_waittimes_, zero.forward_waittimes_);
  std::swap(forward_mb_size_, zero.forward_mb_size_);
  std::swap(receive_msg_count_, zero.receive_msg_count_);
  return zero;
}

} // namespace instrumentation
} // namespace caf
