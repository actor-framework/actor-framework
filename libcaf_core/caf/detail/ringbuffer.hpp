/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace caf {
namespace detail {

// A ringbuffer designed for a single consumer and any number of producers that
// can hold a maximum of `Size - 1` elements.
template <class T, size_t Size>
class ringbuffer {
public:
  using guard_type = std::unique_lock<std::mutex>;

  ringbuffer() : wr_pos_(0), rd_pos_(0) {
    // nop
  }

  void wait_nonempty() {
    // Double-checked locking to reduce contention on mtx_.
    if (!empty())
      return;
    guard_type guard{mtx_};
    while (empty())
      cv_empty_.wait(guard);
  }

  T& front() {
    // Safe to access without lock, because we assume a single consumer.
    return buf_[rd_pos_];
  }

  void pop_front() {
    guard_type guard{mtx_};
    auto rp = rd_pos_.load();
    rd_pos_ = next(rp);
    // Wakeup a waiting producers if the queue became non-full.
    if (rp == next(wr_pos_))
      cv_full_.notify_all();
  }

  void push_back(T&& x) {
    guard_type guard{mtx_};
    while (full())
      cv_full_.wait(guard);
    auto wp = wr_pos_.load();
    buf_[wp] = std::move(x);
    wr_pos_ = next(wp);
    if (rd_pos_ == wp)
      cv_empty_.notify_all();
  }

  bool empty() const noexcept {
    return rd_pos_ == wr_pos_;
  }

  bool full() const noexcept {
    return rd_pos_ == next(wr_pos_);
  }

  size_t size() const noexcept {
    auto rp = rd_pos_.load();
    auto wp = wr_pos_.load();
    if (rp == wp)
      return 0;
    if (rp < wp)
      return wp - rp;
    return Size - rp + wp;
  }

private:
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
  std::atomic<size_t> wr_pos_;

  // Stores the current read position in the ringbuffer.
  std::atomic<size_t> rd_pos_;

  // Stores events in a circular ringbuffer.
  std::array<T, Size> buf_;
};

} // namespace detail
} // namespace caf
