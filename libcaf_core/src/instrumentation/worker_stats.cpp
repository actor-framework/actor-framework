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
#include "caf/detail/hash.hpp"

namespace caf {
namespace instrumentation {

void worker_stats::combine(const worker_stats& rhs) {
  combine_map(behavior_individual_waittime_, rhs.behavior_individual_waittime_);
  combine_map(behavior_aggregate_waittime_, rhs.behavior_aggregate_waittime_);
  combine_map(behavior_individual_mbsize_, rhs.behavior_individual_mbsize_);
  combine_map(behavior_aggregate_mbsize_, rhs.behavior_aggregate_mbsize_);
  combine_map(request_individual_times_, rhs.request_individual_times_);
  combine_map(request_aggregate_times_, rhs.request_aggregate_times_);
  sum_map(send_individual_count_, rhs.send_individual_count_);
  sum_map(send_aggregate_count_, rhs.send_aggregate_count_);
}

static std::string repr(instrumented_actor_id aid) {
  return to_string(aid.type) + " id " + std::to_string(aid.id);
}

const worker_stats::typed_individual& worker_stats::get_individual_behavior_wait_durations() const {
  return behavior_individual_waittime_;
}
const worker_stats::typed_aggregate& worker_stats::get_aggregated_behavior_wait_durations() const {
  return behavior_aggregate_waittime_;
}
const worker_stats::individual& worker_stats::get_individual_mailbox_sizes() const {
  return behavior_individual_mbsize_;
}
const worker_stats::aggregate& worker_stats::get_aggregated_mailbox_sizes() const {
  return behavior_aggregate_mbsize_;
}
const worker_stats::typed_individual& worker_stats::get_individual_request_durations() const {
  return request_individual_times_;
}
const worker_stats::typed_aggregate&  worker_stats::get_aggregate_request_durations() const {
  return request_aggregate_times_;
}
const worker_stats::individual_count& worker_stats::get_individual_send_count() const {
  return send_individual_count_;
}
const worker_stats::aggregate_count& worker_stats::get_aggregate_send_count() const {
  return send_aggregate_count_;
}
std::string worker_stats::to_string() const {
  std::string res;
  res.reserve(4096);
  for (const auto& wt : get_individual_behavior_wait_durations()) {
    res += "WORKER | BEHAVIOR WAIT TIME (individual) | ";
    res += "ACTOR: " + repr(wt.first.actor);
    res += " | ";
    res += "MSGTYPE: " + caf::instrumentation::to_string(wt.first.message);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  for (const auto& wt : get_aggregated_behavior_wait_durations()) {
    res += "WORKER | BEHAVIOR WAIT TIME (aggregate) | ";
    res += "ACTORTYPE: " + caf::instrumentation::to_string(wt.first.actor_type);
    res += " | ";
    res += "MSGTYPE: " + caf::instrumentation::to_string(wt.first.message);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  for (const auto& wt : get_individual_mailbox_sizes()) {
    res += "WORKER | BEHAVIOR MAILBOX SIZE (individual) | ";
    res += "ACTOR: " + repr(wt.first);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  for (const auto& wt : get_aggregated_mailbox_sizes()) {
    res += "WORKER | BEHAVIOR MAILBOX SIZE (aggregate) | ";
    res += "ACTORTYPE: " + caf::instrumentation::to_string(wt.first);
    res += " => ";
    res += wt.second.to_string();
    res += "\n";
  }
  for (const auto& rt : get_individual_request_durations()) {
    res += "WORKER | REQUEST DURATION (individual) | ";
    res += "ACTOR: " + repr(rt.first.actor);
    res += " | ";
    res += caf::instrumentation::to_string(rt.first.message);
    res += " => ";
    res += rt.second.to_string();
    res += "\n";
  }
  for (const auto& rt : get_aggregate_request_durations()) {
    res += "WORKER | REQUEST DURATION (aggregate) | ";
    res += "ACTORTYPE: " + caf::instrumentation::to_string(rt.first.actor_type);
    res += " | ";
    res += caf::instrumentation::to_string(rt.first.message);
    res += " => ";
    res += rt.second.to_string();
    res += "\n";
  }
  for (const auto& st : get_individual_send_count()) {
    res += "WORKER | SEND COUNT (individual) | ";
    res += "ACTOR: " + repr(st.first.actor);
    res += " | ";
    res += caf::instrumentation::to_string(st.first.message);
    res += " => ";
    res += std::to_string(st.second);
    res += "\n";
  }
  for (const auto& st : get_aggregate_send_count()) {
    res += "WORKER | SEND COUNT (aggregate) | ";
    res += "ACTORTYPE: " + caf::instrumentation::to_string(st.first.actor_type);
    res += " | ";
    res += caf::instrumentation::to_string(st.first.message);
    res += " => ";
    res += std::to_string(st.second);
    res += "\n";
  }
  return res;
}

void lockable_worker_stats::record_behavior_individual(instrumented_actor_id aid, msgtype_id mt, int64_t mb_waittime, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  behavior_individual_waittime_[{aid, mt}].record(mb_waittime);
  behavior_individual_mbsize_[aid].record(mb_size);
  behavior_aggregate_waittime_[{aid.type, mt}].record(mb_waittime);
  behavior_aggregate_mbsize_[aid.type].record(mb_size);
}

void lockable_worker_stats::record_behavior_aggregate(actortype_id at, msgtype_id mt, int64_t mb_waittime, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  behavior_aggregate_waittime_[{at, mt}].record(mb_waittime);
  behavior_aggregate_mbsize_[at].record(mb_size);
}

void lockable_worker_stats::record_request_individual(instrumented_actor_id aid, msgtype_id mt, int64_t waittime) {
  std::lock_guard<std::mutex> g(access_mutex_);
  request_individual_times_[{aid, mt}].record(waittime);
}

void lockable_worker_stats::record_request_aggregate(actortype_id at, msgtype_id mt, int64_t waittime) {
  std::lock_guard<std::mutex> g(access_mutex_);
  request_aggregate_times_[{at, mt}].record(waittime);
}
void lockable_worker_stats::record_send_individual(instrumented_actor_id aid, msgtype_id mt) {
  std::lock_guard<std::mutex> g(access_mutex_);
  ++send_individual_count_[{aid, mt}];
}
void lockable_worker_stats::record_send_aggregate(actortype_id at, msgtype_id mt) {
  std::lock_guard<std::mutex> g(access_mutex_);
  ++send_aggregate_count_[{at, mt}];
}

worker_stats lockable_worker_stats::collect() {
  worker_stats zero;
  std::lock_guard<std::mutex> g(access_mutex_);
  std::swap(behavior_individual_waittime_, zero.behavior_individual_waittime_);
  std::swap(behavior_aggregate_waittime_, zero.behavior_aggregate_waittime_);
  std::swap(behavior_individual_mbsize_, zero.behavior_individual_mbsize_);
  std::swap(behavior_aggregate_mbsize_, zero.behavior_aggregate_mbsize_);
  std::swap(request_individual_times_, zero.request_individual_times_);
  std::swap(request_aggregate_times_, zero.request_aggregate_times_);
  std::swap(send_individual_count_, zero.send_individual_count_);
  std::swap(send_aggregate_count_, zero.send_aggregate_count_);
  return zero;
}

} // namespace instrumentation
} // namespace caf