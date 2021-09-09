// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/test_actor_clock.hpp"

#include "caf/logger.hpp"

namespace caf::detail {

test_actor_clock::test_actor_clock() : current_time(duration_type{1}) {
  // This ctor makes sure that the clock isn't at the default-constructed
  // time_point, because begin-of-epoch may have special meaning.
}

disposable test_actor_clock::schedule_periodically(time_point first_run,
                                                   action f,
                                                   duration_type period) {
  CAF_ASSERT(f.ptr() != nullptr);
  schedule.emplace(first_run, schedule_entry{f, period});
  return std::move(f).as_disposable();
}

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
}

bool test_actor_clock::trigger_timeout() {
  CAF_LOG_TRACE(CAF_ARG2("schedule.size", schedule.size()));
  for (;;) {
    if (schedule.empty())
      return false;
    auto i = schedule.begin();
    auto t = i->first;
    if (t > current_time)
      current_time = t;
    if (try_trigger_once())
      return true;
  }
}

size_t test_actor_clock::trigger_timeouts() {
  CAF_LOG_TRACE(CAF_ARG2("schedule.size", schedule.size()));
  if (schedule.empty())
    return 0u;
  size_t result = 0;
  while (trigger_timeout())
    ++result;
  return result;
}

size_t test_actor_clock::advance_time(duration_type x) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG2("schedule.size", schedule.size()));
  CAF_ASSERT(x.count() >= 0);
  current_time += x;
  auto result = size_t{0};
  while (!schedule.empty() && schedule.begin()->first <= current_time)
    if (try_trigger_once())
      ++result;
  return result;
}

bool test_actor_clock::try_trigger_once() {
  auto i = schedule.begin();
  auto t = i->first;
  if (t > current_time)
    return false;
  auto [f, period] = i->second;
  schedule.erase(i);
  if (f.run() == action::transition::success) {
    if (period.count() > 0) {
      auto next = t + period;
      while (next <= current_time)
        next += period;
      schedule.emplace(next, schedule_entry{std::move(f), period});
    }
    return true;
  } else {
    return false;
  }
}

} // namespace caf::detail
