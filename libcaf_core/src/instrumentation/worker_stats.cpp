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

#include "caf/instrumentation/worker_stats.hpp"

namespace caf {
namespace instrumentation {

void worker_stats::combine(const worker_stats& rhs) {
  for (const auto& wt : rhs.msg_waittimes_) {
    msg_waittimes_[wt.first].combine(wt.second);
  }
  for (const auto& mb : rhs.mb_sizes_) {
    mb_sizes_[mb.first].combine(mb.second);
  }
}

std::string worker_stats::to_string() const {
  std::string res;
  res.reserve(4096);
  for (const auto& wt : msg_waittimes_) {
    res += "WORKERS WAIT TIME | ";
    res += caf::instrumentation::to_string(wt.first.first);
    res += " | ";
    res += caf::instrumentation::to_string(wt.first.second);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  for (const auto& mb : mb_sizes_) {
    res += "WORKERS MAILBOX SIZE | ";
    res += caf::instrumentation::to_string(mb.first);
    res += " => ";
    res += mb.second.to_string();
    res += "\n";
  }
  return res;
}

void lockable_worker_stats::record_pre_behavior(actortype_id at, msgtype_id mt, int64_t mb_waittime, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  msg_waittimes_[std::make_pair(at, mt)].record(mb_waittime);
  mb_sizes_[at].record(mb_size);
}

worker_stats lockable_worker_stats::collect() {
  worker_stats zero;
  std::lock_guard<std::mutex> g(access_mutex_);
  std::swap(msg_waittimes_, zero.msg_waittimes_);
  std::swap(mb_sizes_, zero.mb_sizes_);
  return zero;
}

} // namespace instrumentation
} // namespace caf
