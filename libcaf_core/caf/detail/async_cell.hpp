// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/async/execution_context.hpp"
#include "caf/config.hpp"

#include <atomic>
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

  async_cell(const async_cell&) = delete;
  async_cell& operator=(const async_cell&) = delete;

  using atomic_count = std::atomic<size_t>;
  atomic_count promises;
  std::byte padding[CAF_CACHE_LINE_SIZE - sizeof(atomic_count)];

  using event = std::pair<async::execution_context_ptr, action>;
  using event_list = std::vector<event>;
  std::mutex mtx;
  std::variant<none_t, T, error> value;
  event_list events;
};

} // namespace caf::detail
