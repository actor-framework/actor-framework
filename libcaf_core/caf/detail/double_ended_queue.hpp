// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>

namespace caf::detail {

/*
 * A thread-safe, double-ended queue for work-stealing.
 */
template <class T>
class double_ended_queue {
public:
  using value_type = T;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

  // -- for the owner ----------------------------------------------------------

  void prepend(pointer value) {
    CAF_ASSERT(value != nullptr);
    std::unique_lock guard{mtx_};
    items_.push_front(value);
  }

  pointer try_take_head() {
    std::unique_lock guard{mtx_};
    if (!items_.empty()) {
      auto* result = items_.front();
      items_.pop_front();
      return result;
    }
    return nullptr;
  }

  template <class Duration>
  pointer try_take_head(Duration rel_timeout) {
    auto abs_timeout = std::chrono::system_clock::now() + rel_timeout;
    std::unique_lock guard{mtx_};
    while (items_.empty()) {
      if (cv_.wait_until(guard, abs_timeout) == std::cv_status::timeout) {
        return nullptr;
      }
    }
    auto* result = items_.front();
    items_.pop_front();
    return result;
  }

  pointer take_head() {
    std::unique_lock guard{mtx_};
    while (items_.empty()) {
      cv_.wait(guard);
    }
    auto* result = items_.front();
    items_.pop_front();
    return result;
  }

  // Unsafe, since it does not wake up a currently sleeping worker.
  void unsafe_append(pointer value) {
    std::unique_lock guard{mtx_};
    items_.push_back(value);
  }

  // -- for others -------------------------------------------------------------

  void append(pointer value) {
    bool do_notify = false;
    {
      std::unique_lock guard{mtx_};
      do_notify = items_.empty();
      items_.push_back(value);
    }
    if (do_notify) {
      cv_.notify_one();
    }
  }

  pointer try_take_tail() {
    std::unique_lock guard{mtx_};
    if (!items_.empty()) {
      auto* result = items_.back();
      items_.pop_back();
      return result;
    }
    return nullptr;
  }

private:
  std::mutex mtx_;
  std::condition_variable cv_;
  std::list<pointer> items_;
};

} // namespace caf::detail
