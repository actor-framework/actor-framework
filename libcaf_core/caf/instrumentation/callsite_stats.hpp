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
public:
  void record_pre_behavior(int64_t mb_wait_time, size_t mb_size);
  std::string to_string() const;

  stat_stream mb_waittimes() const {
    return mb_waittimes_;
  }

  stat_stream mb_sizes() const {
    return mb_sizes_;
  }

  void combine(const callsite_stats& rhs) {
    mb_waittimes_.combine(rhs.mb_waittimes_);
    mb_sizes_.combine(rhs.mb_sizes_);
  }

private:
  stat_stream mb_waittimes_;
  stat_stream mb_sizes_;
};

} // namespace instrumentation
} // namespace caf

#endif // CAF_WORKER_STATS_HPP
