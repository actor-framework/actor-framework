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

#ifndef CAF_CALLSITE_STATS_HPP
#define CAF_CALLSITE_STATS_HPP

#include "caf/instrumentation/stat_stream.hpp"

#include <cstdint>

namespace caf {
namespace instrumentation {

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
  using mb_size_stream = stat_stream<size_t,
    1,
    8,
    64,
    512,
    4096,
    32768,
    262144
  >;

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

} // namespace instrumentation
} // namespace caf

#endif // CAF_WORKER_STATS_HPP
