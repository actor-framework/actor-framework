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

std::string broker_stats::to_string() const {
  std::string res;
  res.reserve(4096);
  for (const auto& wt : msg_waittimes_) {
    res += "BROKER WAIT TIME | ";
    res += caf::instrumentation::to_string(wt.first);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  res += "BROKER MAILBOX SIZE | ";
  res += mb_size_.to_string();
  res += "\n";
  return res;
}

void lockable_broker_stats::record_broker_forward(msgtype_id mt, int64_t mb_waittime, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  msg_waittimes_[mt].record(mb_waittime);
  mb_size_.record(mb_size);
}

broker_stats lockable_broker_stats::collect() {
  broker_stats zero;
  std::lock_guard<std::mutex> g(access_mutex_);
  std::swap(msg_waittimes_, zero.msg_waittimes_);
  std::swap(mb_size_, zero.mb_size_);
  return zero;
}

} // namespace instrumentation
} // namespace caf
