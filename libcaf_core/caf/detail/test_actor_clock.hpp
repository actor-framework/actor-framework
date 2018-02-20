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

#ifndef CAF_DETAIL_TEST_ACTOR_CLOCK_HPP
#define CAF_DETAIL_TEST_ACTOR_CLOCK_HPP

#include "caf/detail/simple_actor_clock.hpp"

namespace caf {
namespace detail {

class test_actor_clock : public simple_actor_clock {
public:
  time_point current_time;

  time_point now() const noexcept override;

  duration_type difference(atom_value measurement, long units, time_point t0,
                           time_point t1) const noexcept override;

  /// Tries to dispatch the next timeout or delayed message regardless of its
  /// timestamp. Returns `false` if `schedule().empty()`, otherwise `true`.
  bool dispatch_once();

  /// Dispatches all timeouts and delayed messages regardless of their
  /// timestamp. Returns the number of dispatched events.
  size_t dispatch();

  /// Advances the time by `x` and dispatches timeouts and delayed messages.
  void advance_time(duration_type x);

  /// Configures the returned value for `difference`. For example, inserting
  /// `('foo', 120ns)` causes the clock to return `units * 120ns` for any call
  /// to `difference` with `measurement == 'foo'` regardless of the time points
  /// passed to the function.
  std::map<atom_value, duration_type> time_per_unit;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TEST_ACTOR_CLOCK_HPP
