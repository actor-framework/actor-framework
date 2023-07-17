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

frame::data::data(const_byte_span bytes) : data(true, bytes.size()) {
  memcpy(storage(), bytes.data(), bytes.size());
}

frame::data::data(std::string_view str) : data(false, str.size()) {
  memcpy(storage(), str.data(), str.size());
}

frame::data::data(size_t total_size, span<const const_byte_span> bufs)
  : data(true, total_size) {
  std::byte* pos = storage_;
  for (const auto& buf : bufs) {
    if (!buf.empty()) {
      memcpy(pos, buf.data(), buf.size());
      pos += buf.size();
    }
  }
}

frame::data::data(size_t total_size, span<const std::string_view> bufs)
  : data(false, total_size) {
  std::byte* pos = storage_;
  for (const auto& buf : bufs) {
    if (!buf.empty()) {
      memcpy(pos, buf.data(), buf.size());
      pos += buf.size();
    }
  }
}

frame::frame(const_byte_span bytes) {
  init(bytes.size(), bytes);
}

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

void frame::data::deref() noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    this->~data();
    free(this);
  }
}

template <class... Args>
void frame::init(size_t payload_size, Args&&... args) {
  auto vptr = malloc(sizeof(data) + payload_size);
  data_.reset(new (vptr) data(std::forward<Args>(args)...), false);
}

} // namespace caf::net::web_socket
