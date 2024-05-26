// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <array>
#include <condition_variable>
#include <mutex>
#include <optional>

namespace caf::detail {

// A ring buffer backed by an array for a single consumer and any number of
// producers that can hold a maximum of `Size - 1` elements.
template <class T, size_t Size>
class sync_ring_buffer {
public:
  using guard_type = std::unique_lock<std::mutex>;

  sync_ring_buffer() : wr_pos_(0), rd_pos_(0) {
    // nop
  }

  /// Enqueues a new element at the end of the queue. If the queue is full,
  /// this function blocks until space becomes available.
  void push(T&& value) {
    guard_type guard{mtx_};
    while (full())
      cv_full_.wait(guard);
    auto was_empty = empty();
    buf_[wr_pos_] = std::move(value);
    wr_pos_ = next(wr_pos_);
    if (was_empty)
      cv_empty_.notify_all();
  }

  /// Checks whether the queue currently has at least one element.
  bool can_push() const {
    guard_type guard{mtx_};
    return !full();
  }

  /// Dequeues the next element from the queue. If the queue is empty, this
  /// function blocks until an element is available.
  T pop() {
    guard_type guard{mtx_};
    while (empty())
      cv_empty_.wait(guard);
    auto was_full = full();
    auto result = std::move(buf_[rd_pos_]);
    rd_pos_ = next(rd_pos_);
    if (was_full)
      cv_full_.notify_all();
    return result;
  }

  /// Dequeues the next element from the queue. If the queue is empty, this
  /// function blocks until an element is available or the timeout expires.
  template <class TimePoint>
  std::optional<T> try_pop(TimePoint timeout) {
    guard_type guard{mtx_};
    if (empty()) {
      auto pred = [this] { return !empty(); };
      if (!cv_empty_.wait_until(guard, timeout, pred))
        return std::nullopt;
    }
    auto was_full = full();
    auto result = std::move(buf_[rd_pos_]);
    rd_pos_ = next(rd_pos_);
    if (was_full)
      cv_full_.notify_all();
    return result;
  }

private:
  bool empty() const noexcept {
    return rd_pos_ == wr_pos_;
  }

  bool full() const noexcept {
    return rd_pos_ == next(wr_pos_);
  }

  static size_t next(size_t pos) {
    return (pos + 1) % Size;
  }

  // Guards queue_.
  mutable std::mutex mtx_;

  // Signals the empty condition.
  std::condition_variable cv_empty_;

  // Signals the full condition.
  std::condition_variable cv_full_;

  // Stores the current write position in the ringbuffer.
  size_t wr_pos_;

  // Stores the current read position in the ringbuffer.
  size_t rd_pos_;

  // Stores events in a circular ringbuffer.
  std::array<T, Size> buf_;
};

} // namespace caf::detail
