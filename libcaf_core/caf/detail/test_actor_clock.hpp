/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include "caf/detail/simple_actor_clock.hpp"

namespace caf {
namespace detail {

class test_actor_clock : public simple_actor_clock {
public:
  time_point current_time;

  test_actor_clock();

  time_point now() const noexcept override;

  duration_type difference(atom_value measurement, long units, time_point t0,
                           time_point t1) const noexcept override;

  /// Returns whether the actor clock has at least one pending timeout.
  bool has_pending_timeout() const {
    return !schedule_.empty();
  }

  /// Triggers the next pending timeout regardless of its timestamp. Sets
  /// `current_time` to the time point of the triggered timeout unless
  /// `current_time` is already set to a later time.
  /// @returns Whether a timeout was triggered.
  bool trigger_timeout();

  /// Triggers all pending timeouts regardless of their timestamp. Sets
  /// `current_time` to the time point of the latest timeout unless
  /// `current_time` is already set to a later time.
  /// @returns The number of triggered timeouts.
  size_t trigger_timeouts();

  /// Advances the time by `x` and dispatches timeouts and delayed messages.
  /// @returns The number of triggered timeouts.
  size_t advance_time(duration_type x);

  /// Configures the returned value for `difference`. For example, inserting
  /// `('foo', 120ns)` causes the clock to return `units * 120ns` for any call
  /// to `difference` with `measurement == 'foo'` regardless of the time points
  /// passed to the function.
  std::map<atom_value, duration_type> time_per_unit;
};

} // namespace detail
} // namespace caf

