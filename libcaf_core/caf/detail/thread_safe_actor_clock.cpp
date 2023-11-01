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

disposable thread_safe_actor_clock::schedule(time_point abs_time, action f) {
  queue_.push(schedule_entry_ptr{new schedule_entry{abs_time, f}});
  return std::move(f).as_disposable();
}

void thread_safe_actor_clock::run(queue_type* queue) {
  CAF_LOG_TRACE("");
  std::vector<schedule_entry_ptr> tbl;
  tbl.reserve(buffer_size * 2);
  auto is_disposed = [](auto& x) { return !x || x->f.disposed(); };
  auto by_timeout = [](auto& x, auto& y) { return x->t < y->t; };

  for (;;) {
    // Fetch additional scheduling requests from the queue.
    if (tbl.empty()) {
      auto ptr = queue->pop();
      if (!ptr)
        return;
      tbl.emplace_back(std::move(ptr));
    } else {
      auto next_timeout = (*tbl.begin())->t;
      if (auto maybe_ptr = queue->try_pop(next_timeout)) {
        if (!*maybe_ptr)
          return;
        auto insert_pos = std::upper_bound(tbl.begin(), tbl.end(), *maybe_ptr,
                                           by_timeout);
        tbl.insert(insert_pos, std::move(*maybe_ptr));
      }
    }
    // Run all actions that timed out.
    auto n = clock_type::now();
    auto i = tbl.begin();
    for (; i != tbl.end() && (*i)->t <= n; ++i)
      (*i)->f.run();
    // Here, we have [begin, i) be the actions that were executed. Move any
    // already disposed action also to the beginning so that we can erase them
    // at once.
    i = std::stable_partition(i, tbl.end(), is_disposed);
    tbl.erase(tbl.begin(), i);
  }
}

void thread_safe_actor_clock::start_dispatch_loop(caf::actor_system& sys) {
  dispatcher_ = sys.launch_thread("caf.clock", thread_owner::system,
                                  [qptr = &queue_] { run(qptr); });
}

void thread_safe_actor_clock::stop_dispatch_loop() {
  queue_.push(nullptr);
  dispatcher_.join();
}

} // namespace caf::detail
