// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/test_actor_clock.hpp"

#include "caf/logger.hpp"

namespace caf::detail {

test_actor_clock::test_actor_clock() : current_time(duration_type{1}) {
  // This ctor makes sure that the clock isn't at the default-constructed
  // time_point, because that value has special meaning (for the tick_emitter,
  // for example).
}

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
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

} // namespace caf::detail
