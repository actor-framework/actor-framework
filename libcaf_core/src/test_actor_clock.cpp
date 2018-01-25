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

namespace caf {
namespace detail {

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
}

bool test_actor_clock::dispatch_once() {
  if (schedule_.empty())
    return false;
  visitor f{this};
  auto i = schedule_.begin();
  visit(f, i->second);
  schedule_.erase(i);
  return true;
}

size_t test_actor_clock::dispatch() {
  if (schedule_.empty())
    return 0u;
  visitor f{this};
  auto result = schedule_.size();
  for (auto& kvp : schedule_)
    visit(f, kvp.second);
  schedule_.clear();
  return result;
}

void test_actor_clock::advance_time(duration_type x) {
  visitor f{this};
  current_time += x;
  auto i = schedule_.begin();
  while (i != schedule_.end() && i->first <= current_time) {
    visit(f, i->second);
    i = schedule_.erase(i);
  }
}

} // namespace detail
} // namespace caf
