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

#ifndef CAF_DETAIL_TICK_EMITTER_HPP
#define CAF_DETAIL_TICK_EMITTER_HPP

#include <chrono>

#include "caf/config.hpp"

namespace caf {
namespace detail {

/// Converts realtime into a series of ticks, whereas each tick represents a
/// preconfigured timespan. For example, a tick emitter configured with a
/// timespan of 25ms generates a tick every 25ms after starting it.
class tick_emitter {
public:
  // -- member types -----------------------------------------------------------

  /// Discrete point in time.
  using clock_type = std::chrono::steady_clock;

  /// Discrete point in time.
  using time_point = typename clock_type::time_point;

  /// Difference between two points in time.
  using duration_type = typename time_point::duration;

  // -- constructors, destructors, and assignment operators --------------------

  tick_emitter(time_point now)
      : start_(std::move(now)),
        interval_(0),
        last_tick_id_(0) {
    // nop
  }

  void interval(duration_type x) {
    interval_ = x;
  }

  template <class F>
  void update(time_point now, F& consumer) {
    CAF_ASSERT(interval_.count() != 0);
    auto diff = now - start_;
    auto current_tick_id = diff.count() / interval_.count();
    while (last_tick_id_ < current_tick_id)
      consumer(++last_tick_id_);
  }

private:
  time_point start_;
  duration_type interval_;
  long last_tick_id_;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TICK_EMITTER_HPP

