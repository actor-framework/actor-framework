// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>

namespace caf::test {

/// A thread-safe counter that can be used to wait for a specific value.
class synchronized_counter {
public:
  /// Increments the counter and notifies all waiting threads.
  void inc() {
    std::unique_lock<std::mutex> guard{mtx_};
    ++count_;
    cv_.notify_all();
  }

  /// Waits until the counter reaches the specified value or a timeout occurs.
  template <class Rep, class Period>
  bool await(size_t value, std::chrono::duration<Rep, Period> timeout) {
    std::unique_lock<std::mutex> guard{mtx_};
    return cv_.wait_for(guard, timeout, [&] { return count_ == value; });
  }

  /// Waits until the counter reaches the specified value or a timeout occurs.
  template <class Clock, class Duration>
  bool await(size_t value, std::chrono::time_point<Clock, Duration> timeout) {
    std::unique_lock<std::mutex> guard{mtx_};
    return cv_.wait_until(guard, timeout, [&] { return count_ == value; });
  }

  /// Creates a new instance of `synchronized_counter` and returns a shared
  /// pointer to it.
  static auto make() {
    return std::make_shared<synchronized_counter>();
  }

private:
  std::mutex mtx_;
  std::condition_variable cv_;
  size_t count_ = 0;
};

/// A shared pointer to a synchronized_counter with shared ownership semantics.
using synchronized_counter_ptr = std::shared_ptr<synchronized_counter>;

} // namespace caf::test
