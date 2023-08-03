// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <algorithm>
#include <memory>

namespace caf::detail {

// A simple ring buffer implementation
template <class T>
class ring_buffer {
public:
  explicit ring_buffer(size_t max_size)
    : max_size_(max_size), size_(0), write_pos_(0) {
    if (max_size > 0)
      buf_ = std::make_unique<T[]>(max_size);
  }

  ring_buffer(ring_buffer&& other) noexcept = default;

  ring_buffer& operator=(ring_buffer&& other) noexcept = default;

  explicit ring_buffer(const ring_buffer& other) {
    max_size_ = other.max_size_;
    write_pos_ = other.write_pos_;
    size_ = other.size_;
    buf_ = std::make_unique<T[]>(max_size_);
    if (max_size_ > 0)
      std::copy(other.buf_.get(), other.buf_.get() + other.max_size_,
                buf_.get());
  }

  ring_buffer& operator=(const ring_buffer& other) {
    ring_buffer copy_ring_buffer{other.max_size_};
    if (max_size_ > 0)
      std::copy(other.buf_.get(), other.buf_.get() + other.max_size_,
                copy_ring_buffer.buf_);
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

private:
  /// write index for the buffer, new elements will be added in this index
  size_t write_pos_;
  /// maximum size of the buffer
  size_t max_size_;
  /// number of elements in the buffer
  size_t size_;
  /// Stores events in a circular ringbuffer
  std::unique_ptr<T[]> buf_;
};

} // namespace caf::detail
