// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/binary/frame.hpp"

#include <cstring>
#include <new>
#include <numeric>

namespace caf::net::binary {

namespace {

template <class Buffer>
size_t calc_total_size(span<const Buffer> buffers) {
  auto fn = [](size_t n, const Buffer& buf) { return n + buf.size(); };
  return std::accumulate(buffers.begin(), buffers.end(), size_t{0}, fn);
}

} // namespace

frame::frame(const_byte_span buf) {
  init(buf.size(), buf);
}

frame::frame(caf::span<const const_byte_span> bufs) {
  auto payload_size = calc_total_size(bufs);
  init(payload_size, payload_size, bufs);
}

template <class... Args>
void frame::init(size_t payload_size, Args&&... args) {
  using data_t = web_socket::frame::data;
  auto vptr = malloc(sizeof(data_t) + payload_size);
  data_.reset(new (vptr) data_t(std::forward<Args>(args)...), false);
}

} // namespace caf::net::binary
