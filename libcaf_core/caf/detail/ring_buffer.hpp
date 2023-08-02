// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <memory>

namespace caf::detail {

// A simple ring buffer implementation
template <class T>
class ring_buffer {
public:
  ring_buffer(size_t buffer_size)
    : write_pos_(0),
      max_size_(buffer_size),
      size_(0),
      buf_(std::unique_ptr<T[]>(new T[buffer_size])) {
    // nop
  }

  ring_buffer(const ring_buffer& other)
    : write_pos_(other.write_pos_),
      max_size_(other.max_size_),
      size_(other.size_),
      buf_(std::unique_ptr<T[]>(new T[other.max_size_])) {
    for (int i = 0; i < max_size_; ++i)
      buf_[i] = other.buf_[i];
  }

  T& front() {
    return buf_[(write_pos_ + max_size_ - size_ + 1) % max_size_];
  }

  void pop_front() {
    if (empty())
      return;
    --size_;
  }

  void push_back(const T& x) {
    if (max_size_ <= 0)
      return;
    write_pos_ = (write_pos_ + 1) % max_size_;
    buf_[write_pos_] = x;
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
  // current head, indicates the index of the front of the buffer
  std::size_t write_pos_;
  // current tail, this is the index where new items will be added
  std::size_t max_size_;
  // number of elements in the buffer
  std::size_t size_;
  // Stores events in a circular ringbuffer
  std::unique_ptr<T[]> buf_;
};

} // namespace caf::detail
