// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace caf::detail {

// Like std::latch, but allows waiting with a timeout.
class CAF_CORE_EXPORT latch {
public:
  explicit latch(ptrdiff_t value) : count_(value) {
    // nop
  }

  latch(const latch&) = delete;

  latch& operator=(const latch&) = delete;

  void count_down_and_wait();

  void wait();

  void count_down();

  bool is_ready() const noexcept;

  template <class Rep, class Period>
  bool wait_for(const std::chrono::duration<Rep, Period>& rel_timeout) {
    return wait_until(std::chrono::steady_clock::now() + rel_timeout);
  }

  template <class Clock, class Duration>
  bool wait_until(const std::chrono::time_point<Clock, Duration>& abs_timeout) {
    std::unique_lock guard{mtx_};
    return cv_.wait_until(guard, abs_timeout, [&] { return count_ == 0; });
  }

  template <class Rep, class Period>
  bool count_down_and_wait_for(
    const std::chrono::duration<Rep, Period>& rel_timeout) {
    return count_down_and_wait_until(std::chrono::steady_clock::now()
                                     + rel_timeout);
  }

  template <class Clock, class Duration>
  bool count_down_and_wait_until(
    const std::chrono::time_point<Clock, Duration>& abs_timeout) {
    std::unique_lock guard{mtx_};
    if (--count_ == 0) {
      cv_.notify_all();
      return true;
    }
    return cv_.wait_until(guard, abs_timeout, [&] { return count_ == 0; });
  }

private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  ptrdiff_t count_;
};

} // namespace caf::detail
