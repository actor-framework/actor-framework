// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/frame.hpp"

#include <cstring>
#include <new>

namespace caf::net::web_socket {

frame::frame(const_byte_span bytes) {
  alloc(true, bytes.size());
  memcpy(data_->storage(), bytes.data(), bytes.size());
}

frame::frame(std::string_view text) {
  alloc(false, text.size());
  memcpy(data_->storage(), text.data(), text.size());
}

const_byte_span frame::as_binary() const noexcept {
  return {data_->storage(), data_->size()};
}

std::string_view frame::as_text() const noexcept {
  return {reinterpret_cast<const char*>(data_->storage()), data_->size()};
}

void frame::data::deref() noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    this->~data();
    free(this);
  }
}

void frame::alloc(bool is_binary, size_t num_bytes) {
  auto total_size = sizeof(data) + num_bytes;
  auto vptr = malloc(total_size);
  data_.reset(new (vptr) data(is_binary, num_bytes), false);
}

} // namespace caf::net::web_socket
