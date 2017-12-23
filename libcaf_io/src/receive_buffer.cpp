/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <algorithm>

#include "caf/config.hpp"

#include "caf/io/network/receive_buffer.hpp"

namespace {

constexpr size_t min_size = 1;

} // namespace anonymous

namespace caf {
namespace io {
namespace network {

receive_buffer::receive_buffer()
    : buffer_(nullptr),
      capacity_(0),
      size_(0) {
  // nop
}

receive_buffer::receive_buffer(size_type size)
    : buffer_(new value_type[size]),
      capacity_(size),
      size_(0) {
  // nop
}

receive_buffer::receive_buffer(receive_buffer&& other) noexcept
    : capacity_(std::move(other.capacity_)),
      size_(std::move(other.size_)) {
  buffer_ = std::move(other.buffer_);
  other.size_ = 0;
  other.capacity_ = 0;
  other.buffer_.reset();
}

receive_buffer::receive_buffer(const receive_buffer& other)
    : capacity_(other.capacity_),
      size_(other.size_) {
  if (other.size_ == 0) {
    buffer_.reset();
  } else {
    buffer_.reset(new value_type[other.size_]);
    std::copy(other.cbegin(), other.cend(), buffer_.get());
  }
}

receive_buffer& receive_buffer::operator=(receive_buffer&& other) noexcept {
  size_ = std::move(other.size_);
  capacity_ = std::move(other.capacity_);
  buffer_ = std::move(other.buffer_);
  other.clear();
  return *this;
}

receive_buffer& receive_buffer::operator=(const receive_buffer& other) {
  size_ = other.size_;
  capacity_ = other.capacity_;
  if (other.size_ == 0) {
    buffer_.reset();
  } else {
    buffer_.reset(new value_type[other.size_]);
    std::copy(other.cbegin(), other.cend(), buffer_.get());
  }
  return *this;
}

void receive_buffer::resize(size_type new_size) {
  if (new_size > capacity_)
    increase_by(new_size - capacity_);
  size_ = new_size;
}

void receive_buffer::reserve(size_type new_size) {
  if (new_size > capacity_)
    increase_by(new_size  - capacity_);
}

void receive_buffer::shrink_to_fit() {
  if (capacity_ > size_)
    shrink_by(capacity_ - size_);
}

void receive_buffer::clear() {
  size_ = 0;
  buffer_ptr new_buffer{new value_type[capacity_]};
  std::swap(buffer_, new_buffer);
}

void receive_buffer::swap(receive_buffer& other) noexcept {
  std::swap(capacity_, other.capacity_);
  std::swap(size_, other.size_);
  std::swap(buffer_, other.buffer_);
}

void receive_buffer::push_back(value_type value) {
  if (size_ == capacity_)
    increase_by(std::max(capacity_, min_size));
  buffer_.get()[size_] = value;
  ++size_;
}

void receive_buffer::increase_by(size_t bytes) {
  if (bytes == 0)
    return;
  if (!buffer_) {
    buffer_.reset(new value_type[bytes]);
  } else {
    buffer_ptr new_buffer{new value_type[capacity_ + bytes]};
    std::copy(begin(), end(), new_buffer.get());
    std::swap(buffer_, new_buffer);
  }
  capacity_ += bytes;
}

void receive_buffer::shrink_by(size_t bytes) {
  CAF_ASSERT(bytes <= capacity_);
  size_t new_size = capacity_ - bytes;
  if (new_size == 0) {
    buffer_.reset();
  } else {
    buffer_ptr new_buffer{new value_type[new_size]};
    std::copy(begin(), begin() + new_size, new_buffer.get());
    std::swap(buffer_, new_buffer);
  }
  capacity_ = new_size;
}

receive_buffer::iterator receive_buffer::insert(iterator pos, value_type value) {
  if (size_ == capacity_) {
    auto dist = (pos == nullptr) ? 0 : std::distance(begin(), pos);
    increase_by(std::max(capacity_, min_size));
    pos = begin() + dist;
  }
  std::copy(pos, end(), pos + 1);
  *pos = value;
  ++size_;
  return pos;
}

} // namepsace network
} // namespace io
} // namespace caf
