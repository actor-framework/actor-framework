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

#ifndef CAF_WORKER_STATS_HPP
#define CAF_WORKER_STATS_HPP

#include <array>
#include <atomic>
#include <string>
#include <chrono>
#include <cstdint>
#include <unordered_map>

#include "caf/timestamp.hpp"

#include "metric.hpp"
#include "stat_stream.hpp"
#include "signature_registry.hpp"
#include "instrumentation_ids.hpp"

namespace caf {
namespace instrumentation {

static const int max_instrumented_mailbox_size = 64;

/// Instrumentation stats aggregated per-worker-per-callsite.
class callsite_stats {
  using mb_waittime_stream = stat_stream<int64_t,
    10000ull,      // 10 us
    100000ull,     // 100 us
    1000000ull,    // 1 ms
    10000000ull,   // 10 ms
    100000000ull,  // 100 ms
    1000000000ull, // 1s
    10000000000ull // 10s
  >;
  using mb_size_stream = stat_stream<size_t, 1, 2, 4, 8, 16, 32, max_instrumented_mailbox_size>;

public:
  void record_pre_behavior(int64_t mb_wait_time, size_t mb_size);
  std::string to_string() const;

  mb_waittime_stream mb_waittimes() const {
    return mb_waittimes_;
  }

  mb_size_stream mb_sizes() const {
    return mb_sizes_;
  }

private:
  mb_waittime_stream mb_waittimes_;
  mb_size_stream mb_sizes_;
};

/// Instrumentation stats aggregated per-worker for all callsites.
class worker_stats {
public:
  worker_stats() : clear_request_(false) {
  }

  void record_pre_behavior(actortype_id at, callsite_id cs, int64_t mb_wait_time, size_t mb_size);
  std::vector<metric> collect_metrics() const;
  std::string to_string() const;

  signature_registry& registry() {
    return registry_;
  }

  void request_clear() {
    clear_request_ = true;
  }

private:
  void clear_if_requested();

  std::atomic<bool> clear_request_;
  std::unordered_map<actortype_id, std::unordered_map<callsite_id, callsite_stats>> callsite_stats_; // indexed by actortype_id and then callsite_id
  signature_registry registry_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_WORKER_STATS_HPP
