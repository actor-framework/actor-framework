// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/thread_safe_actor_clock.hpp"

#include "caf/actor_control_block.hpp"
#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/system_messages.hpp"
#include "caf/thread_owner.hpp"

namespace caf::detail {

thread_safe_actor_clock::thread_safe_actor_clock() {
  tbl_.reserve(buffer_size * 2);
}

disposable thread_safe_actor_clock::schedule(time_point abs_time, action f) {
  queue_.emplace_back(schedule_entry_ptr{new schedule_entry{abs_time, f}});
  return std::move(f).as_disposable();
}

void thread_safe_actor_clock::run() {
  CAF_LOG_TRACE("");
  auto is_disposed = [](auto& x) { return !x || x->f.disposed(); };
  auto by_timeout = [](auto& x, auto& y) { return x->t < y->t; };
  while (running_) {
    // Fetch additional scheduling requests from the queue.
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
    // Run all actions that timed out.
    auto n = now();
    auto i = tbl_.begin();
    for (; i != tbl_.end() && (*i)->t <= n; ++i)
      (*i)->f.run();
    // Here, we have [begin, i) be the actions that were executed. Move any
    // already disposed action also to the beginning so that we can erase them
    // at once.
    i = std::stable_partition(i, tbl_.end(), is_disposed);
    tbl_.erase(tbl_.begin(), i);
  }
}

void thread_safe_actor_clock::start_dispatch_loop(caf::actor_system& sys) {
  dispatcher_ = sys.launch_thread("caf.clock", thread_owner::system,
                                  [this] { run(); });
}

void thread_safe_actor_clock::stop_dispatch_loop() {
  schedule(make_action([this] { running_ = false; }));
  dispatcher_.join();
}

} // namespace caf::detail
