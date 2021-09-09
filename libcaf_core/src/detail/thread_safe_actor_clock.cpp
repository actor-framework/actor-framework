// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/thread_safe_actor_clock.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"

namespace caf::detail {

thread_safe_actor_clock::thread_safe_actor_clock() {
  tbl_.reserve(buffer_size * 2);
}

disposable
thread_safe_actor_clock::schedule_periodically(time_point first_run, action f,
                                               duration_type period) {
  queue_.emplace_back(new schedule_entry{first_run, f, period});
  return std::move(f).as_disposable();
}

void thread_safe_actor_clock::run() {
  CAF_LOG_TRACE("");
  auto is_disposed = [](auto& x) { return !x || x->f.disposed(); };
  auto by_timeout = [](auto& x, auto& y) { return x->t < y->t; };
  while (running_) {
    if (tbl_.empty()) {
      queue_.wait_nonempty();
      queue_.get_all(std::back_inserter(tbl_));
      std::sort(tbl_.begin(), tbl_.end(), by_timeout);
    } else {
      auto next_timeout = (*tbl_.begin())->t;
      if (queue_.wait_nonempty(next_timeout)) {
        queue_.get_all(std::back_inserter(tbl_));
        std::sort(tbl_.begin(), tbl_.end(), by_timeout);
      }
    }
    auto n = now();
    for (auto i = tbl_.begin(); i != tbl_.end() && (*i)->t <= n; ++i) {
      auto& entry = **i;
      if (entry.f.run() == action::transition::success) {
        if (entry.period.count() > 0) {
          auto next = entry.t + entry.period;
          while (next <= n) {
            CAF_LOG_WARNING("clock lagging behind, skipping a tick!");
            next += entry.period;
          }
        } else {
          i->reset(); // Remove from tbl_ after the for-loop body.
        }
      } else {
        i->reset(); // Remove from tbl_ after the for-loop body.
      }
    }
    tbl_.erase(std::remove_if(tbl_.begin(), tbl_.end(), is_disposed),
               tbl_.end());
  }
}

void thread_safe_actor_clock::start_dispatch_loop(caf::actor_system& sys) {
  dispatcher_ = sys.launch_thread("caf.clock", [this] { run(); });
}

void thread_safe_actor_clock::stop_dispatch_loop() {
  schedule(make_action([this] { running_ = false; }));
  dispatcher_.join();
}

} // namespace caf::detail
