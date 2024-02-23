// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/config.hpp"
#include "caf/none.hpp"
#include "caf/unit.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>

namespace caf::detail {

/// Implementation detail for @ref async::future and @ref async::promise.
template <class T>
struct async_cell {
  async_cell() : promises(1) {
    // Make room for a couple of events to avoid frequent heap allocations in
    // critical sections. We could also use a custom allocator to use
    // small-buffer-optimization.
    events.reserve(8);
  }

  bool subscribe(async::execution_context_ptr ctx, action callback) {
    auto ev = event{std::move(ctx), std::move(callback)};
    { // Critical section.
      std::unique_lock guard{mtx};
      if (std::holds_alternative<none_t>(value)) {
        events.push_back(std::move(ev));
        return true;
      } else {
        return false;
      }
    }
  }

  async_cell(const async_cell&) = delete;
  async_cell& operator=(const async_cell&) = delete;

  using atomic_count = std::atomic<size_t>;
  atomic_count promises;
  std::byte padding[CAF_CACHE_LINE_SIZE - sizeof(atomic_count)];

  using event = std::pair<async::execution_context_ptr, action>;
  using event_list = std::vector<event>;
  using value_type = std::conditional_t<std::is_void_v<T>, unit_t, T>;
  std::mutex mtx;
  std::variant<none_t, value_type, error> value;
  event_list events;
};

} // namespace caf::detail
