// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"

#include <vector>

namespace caf::detail {

// A simple ring buffer implementation
template <class T>
class ring_buffer {
public:
  ring_buffer(size_t buffer_size)
    : curr_front_(0),
      curr_back_(0),
      element_count_(0),
      buf_(std::vector<T>(buffer_size)) {
    // nop
  }

  T& front() {
    return buf_[curr_front_ % buf_.size()];
  }

  void pop_front() {
    if (empty())
      return;
    curr_front_++;
    element_count_--;
  }

  void push_back(const T& x) {
    buf_[curr_back_ % buf_.size()] = x;
    curr_back_++;
    if (full())
      curr_front_++;
    else
      element_count_++;
  }

  bool full() const noexcept {
    return element_count_ == buf_.size();
  }

  bool empty() const noexcept {
    return curr_back_ == curr_front_;
  }

  size_t size() const noexcept {
    return element_count_;
  }

private:
  // current head, indicates the index of the front of the buffer
  std::size_t curr_front_;
  // current tail, this is the index where new items will be added
  std::size_t curr_back_;
  // number of elements in the buffer
  std::size_t element_count_;
  // Stores events in a circular ringbuffer
  std::vector<T> buf_;
};

} // namespace caf::detail
