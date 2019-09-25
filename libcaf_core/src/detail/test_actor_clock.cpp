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

#include "caf/detail/test_actor_clock.hpp"

#include "caf/logger.hpp"

namespace caf {
namespace detail {

test_actor_clock::test_actor_clock() : current_time(duration_type{1}) {
  // This ctor makes sure that the clock isn't at the default-constructed
  // time_point, because that value has special meaning (for the tick_emitter,
  // for example).
}

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
}

test_actor_clock::duration_type
test_actor_clock::difference(atom_value measurement, long units, time_point t0,
                             time_point t1) const noexcept {
  auto i = time_per_unit.find(measurement);
  if (i != time_per_unit.end())
    return units * i->second;
  return t0 == t1 ? duration_type{1} : t1 - t0;
}

bool test_actor_clock::trigger_timeout() {
  CAF_LOG_TRACE(CAF_ARG2("schedule.size", schedule_.size()));
  if (schedule_.empty())
    return false;
  auto i = schedule_.begin();
  auto tout = i->first;
  if (tout > current_time)
    current_time = tout;
  auto ptr = std::move(i->second);
  schedule_.erase(i);
  auto backlink = ptr->backlink;
  if (backlink != actor_lookup_.end())
    actor_lookup_.erase(backlink);
  ship(*ptr);
  return true;
}

size_t test_actor_clock::trigger_timeouts() {
  CAF_LOG_TRACE(CAF_ARG2("schedule.size", schedule_.size()));
  if (schedule_.empty())
    return 0u;
  size_t result = 0;
  while (trigger_timeout())
    ++result;
  return result;
}

size_t test_actor_clock::advance_time(duration_type x) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG2("schedule.size", schedule_.size()));
  CAF_ASSERT(x.count() >= 0);
  current_time += x;
  return trigger_expired_timeouts();
}

} // namespace detail
} // namespace caf
