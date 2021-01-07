// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/tick_emitter.hpp"

#include "caf/logger.hpp"

namespace caf::detail {

tick_emitter::tick_emitter()
  : start_(duration_type{0}), interval_(0), last_tick_id_(0) {
  // nop
}

tick_emitter::tick_emitter(time_point now) : tick_emitter() {
  start(std::move(now));
}

bool tick_emitter::started() const {
  return start_.time_since_epoch().count() != 0;
}

void tick_emitter::start(time_point now) {
  CAF_LOG_TRACE(CAF_ARG(now));
  start_ = std::move(now);
}

void tick_emitter::stop() {
  CAF_LOG_TRACE("");
  start_ = time_point{duration_type{0}};
}

void tick_emitter::interval(duration_type x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  interval_ = x;
}

size_t
tick_emitter::timeouts(time_point now, std::initializer_list<size_t> periods) {
  CAF_LOG_TRACE(CAF_ARG(now)
                << CAF_ARG(periods) << CAF_ARG(interval_) << CAF_ARG(start_));
  CAF_ASSERT(now >= start_);
  size_t result = 0;
  auto f = [&](size_t tick) {
    size_t n = 0;
    for (auto p : periods) {
      if (tick % p == 0)
        result |= 1l << n;
      ++n;
    }
  };
  update(now, f);
  return result;
}

tick_emitter::time_point
tick_emitter::next_timeout(time_point t,
                           std::initializer_list<size_t> periods) {
  CAF_ASSERT(interval_.count() != 0);
  auto is_trigger = [&](size_t tick_id) {
    for (auto period : periods)
      if (tick_id % period == 0)
        return true;
    return false;
  };
  auto diff = t - start_;
  auto this_tick = static_cast<size_t>(diff.count() / interval_.count());
  auto tick_id = this_tick + 1;
  while (!is_trigger(tick_id))
    ++tick_id;
  return start_ + (interval_ * tick_id);
}

} // namespace caf::detail
