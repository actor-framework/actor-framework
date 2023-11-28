// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/web_socket/frame.hpp"

#include <cstring>
#include <new>
#include <numeric>

namespace caf::net::web_socket {

namespace {

template <class Buffer>
size_t calc_total_size(span<const Buffer> buffers) {
  auto fn = [](size_t n, const Buffer& buf) { return n + buf.size(); };
  return std::accumulate(buffers.begin(), buffers.end(), size_t{0}, fn);
}

} // namespace

frame::frame(std::string_view text) {
  init(text.size(), text);
}

frame::frame(caf::span<const const_byte_span> buffers) {
  auto payload_size = calc_total_size(buffers);
  init(payload_size, payload_size, buffers);
}

frame::frame(caf::span<const std::string_view> buffers) {
  auto payload_size = calc_total_size(buffers);
  init(payload_size, payload_size, buffers);
}

const_byte_span frame::as_binary() const noexcept {
  using res_t = const_byte_span;
  return data_ ? res_t{data_->storage(), data_->size()} : res_t{};
}

std::string_view frame::as_text() const noexcept {
  using res_t = std::string_view;
  return data_ ? res_t{reinterpret_cast<const char*>(data_->storage()),
                       data_->size()}
               : res_t{};
}

template <class... Args>
void frame::init(size_t payload_size, Args&&... args) {
  auto vptr = malloc(sizeof(chunk::data) + payload_size);
  data_.reset(new (vptr) chunk::data(std::forward<Args>(args)...), false);
}

} // namespace caf::net::web_socket
