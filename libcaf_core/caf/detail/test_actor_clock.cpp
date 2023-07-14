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

disposable test_actor_clock::schedule(time_point abs_time, action f) {
  CAF_ASSERT(f.ptr() != nullptr);
  actions.emplace(abs_time, f);
  return std::move(f).as_disposable();
}

test_actor_clock::time_point test_actor_clock::now() const noexcept {
  return current_time;
}

bool test_actor_clock::trigger_timeout() {
  CAF_LOG_TRACE(CAF_ARG2("actions.size", actions.size()));
  for (;;) {
    if (actions.empty())
      return false;
    auto i = actions.begin();
    auto t = i->first;
    if (t > current_time)
      current_time = t;
    if (try_trigger_once())
      return true;
  }
}

size_t test_actor_clock::trigger_timeouts() {
  CAF_LOG_TRACE(CAF_ARG2("actions.size", actions.size()));
  if (actions.empty())
    return 0u;
  size_t result = 0;
  while (trigger_timeout())
    ++result;
  return result;
}

size_t test_actor_clock::advance_time(duration_type x) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG2("actions.size", actions.size()));
  CAF_ASSERT(x.count() >= 0);
  current_time += x;
  auto result = size_t{0};
  while (!actions.empty() && actions.begin()->first <= current_time)
    if (try_trigger_once())
      ++result;
  return result;
}

bool test_actor_clock::try_trigger_once() {
  for (;;) {
    if (actions.empty())
      return false;
    auto i = actions.begin();
    auto [t, f] = *i;
    if (t > current_time)
      return false;
    actions.erase(i);
    if (!f.disposed()) {
      f.run();
      return true;
    }
  }
}

} // namespace caf::detail
