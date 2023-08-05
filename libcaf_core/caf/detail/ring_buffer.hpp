// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <algorithm>
#include <memory>

namespace caf::detail {

/// A simple ring buffer implementation.
template <class T>
class ring_buffer {
public:
  explicit ring_buffer(size_t max_size) : max_size_(max_size) {
    if (max_size > 0)
      buf_ = std::make_unique<T[]>(max_size);
  }

  ring_buffer(ring_buffer&& other) noexcept = default;

  ring_buffer& operator=(ring_buffer&& other) noexcept = default;

  ring_buffer(const ring_buffer& other) {
    max_size_ = other.max_size_;
    write_pos_ = other.write_pos_;
    size_ = other.size_;
    if (max_size_ > 0) {
      buf_ = std::make_unique<T[]>(max_size_);
      std::copy(other.buf_.get(), other.buf_.get() + other.max_size_,
                buf_.get());
    }
  }

  ring_buffer& operator=(const ring_buffer& other) {
    ring_buffer tmp{other};
    swap(*this, tmp);
    return *this;
  }

  T& front() {
    return buf_[(write_pos_ + max_size_ - size_) % max_size_];
  }

  void pop_front() {
    CAF_ASSERT(!empty());
    --size_;
  }

  void push_back(const T& x) {
    if (max_size_ == 0)
      return;
    buf_[write_pos_] = x;
    write_pos_ = (write_pos_ + 1) % max_size_;
    if (!full())
      ++size_;
  }

  bool full() const noexcept {
    return size_ == max_size_;
  }

  bool empty() const noexcept {
    return size_ == 0;
  }

  size_t size() const noexcept {
    return size_;
  }

  friend void swap(ring_buffer& first, ring_buffer& second) noexcept {
    using std::swap;
    swap(first.buf_, second.buf_);
    swap(first.size_, second.size_);
    swap(first.max_size_, second.max_size_);
    swap(first.write_pos_, second.write_pos_);
  }

private:
  /// The index for writing new elements.
  size_t write_pos_ = 0;

  /// Maximum size of the buffer.
  size_t max_size_;

  /// The number of elements in the buffer currently.
  size_t size_ = 0;

  /// Stores events in a circular ringbuffer.
  std::unique_ptr<T[]> buf_;
};

} // namespace caf::detail
