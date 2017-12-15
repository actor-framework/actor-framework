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

void worker_stats::record_pre_behavior(actortype_id at, callsite_id cs, int64_t mb_wait_time, size_t mb_size) {
  std::lock_guard<std::mutex> g(access_mutex_);
  callsite_stats_[at][cs].record_pre_behavior(mb_wait_time, mb_size);
}

std::vector<metric> worker_stats::collect_metrics() {
  std::lock_guard<std::mutex> g(access_mutex_);
  std::vector<metric> metrics;
  for (const auto& by_actor : callsite_stats_) {
    auto actortype = registry_.identify_actortype(by_actor.first);
    for (const auto& by_callsite : by_actor.second) {
      auto callsite = registry_.identify_simple_signature(by_callsite.first);
      metrics.emplace_back(metric::type::pre_behavior, actortype, callsite, by_callsite.second); // TODO better type
    }
  }
  callsite_stats_.clear();
  return metrics;
}

std::string worker_stats::to_string() const {
    std::string res;
    for (const auto& by_actor : callsite_stats_) {
      for (const auto& by_callsite : by_actor.second) {
        res += "ACTORTYPE " + registry_.identify_actortype(by_actor.first);
        res += " CALLSITE " + registry_.identify_simple_signature(by_callsite.first);
        res += " => " + by_callsite.second.to_string();
        res += "\n";
      }
    }
    return res;
}

} // namespace instrumentation
} // namespace caf
