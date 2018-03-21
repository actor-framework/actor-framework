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

#include <algorithm>

#include "caf/config.hpp"

#include "caf/io/network/receive_buffer.hpp"

namespace {

constexpr size_t min_size = 1;

} // namespace anonymous

namespace caf {
namespace io {
namespace network {

receive_buffer::receive_buffer() noexcept
    : buffer_(nullptr),
      capacity_(0),
      size_(0) {
  // nop
}

receive_buffer::receive_buffer(size_type count) : receive_buffer() {
  resize(count);
}

receive_buffer::receive_buffer(receive_buffer&& other) noexcept
    : receive_buffer() {
  swap(other);
}

receive_buffer::receive_buffer(const receive_buffer& other)
    : receive_buffer(other.size()) {
  std::copy(other.cbegin(), other.cend(), buffer_.get());
}

receive_buffer& receive_buffer::operator=(receive_buffer&& other) noexcept {
  receive_buffer tmp{std::move(other)};
  swap(tmp);
  return *this;
}

receive_buffer& receive_buffer::operator=(const receive_buffer& other) {
  resize(other.size());
  std::copy(other.cbegin(), other.cend(), buffer_.get());
  return *this;
}

void receive_buffer::resize(size_type new_size) {
  reserve(new_size);
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
}

void receive_buffer::swap(receive_buffer& other) noexcept {
  using std::swap;
  swap(capacity_, other.capacity_);
  swap(size_, other.size_);
  swap(buffer_, other.buffer_);
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
    using std::swap;
    buffer_ptr new_buffer{new value_type[capacity_ + bytes]};
    std::copy(begin(), end(), new_buffer.get());
    swap(buffer_, new_buffer);
  }
  capacity_ += bytes;
}

void receive_buffer::shrink_by(size_t bytes) {
  CAF_ASSERT(bytes <= capacity_);
  size_t new_size = capacity_ - bytes;
  if (new_size == 0) {
    buffer_.reset();
  } else {
    using std::swap;
    buffer_ptr new_buffer{new value_type[new_size]};
    std::copy(begin(), begin() + new_size, new_buffer.get());
    swap(buffer_, new_buffer);
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
