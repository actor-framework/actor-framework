// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/chunk.hpp"

#include <cstring>
#include <new>
#include <numeric>

namespace caf {

namespace {

template <class Buffer>
size_t calc_total_size(span<const Buffer> buffers) {
  auto fn = [](size_t n, const Buffer& buf) { return n + buf.size(); };
  return std::accumulate(buffers.begin(), buffers.end(), size_t{0}, fn);
}

} // namespace

chunk::data::data(const_byte_span bytes) : data(true, bytes.size()) {
  memcpy(storage(), bytes.data(), bytes.size());
}

chunk::data::data(std::string_view str) : data(false, str.size()) {
  memcpy(storage(), str.data(), str.size());
}

chunk::data::data(size_t total_size, span<const const_byte_span> bufs)
  : data(true, total_size) {
  std::byte* pos = storage_;
  for (const auto& buf : bufs) {
    if (!buf.empty()) {
      memcpy(pos, buf.data(), buf.size());
      pos += buf.size();
    }
  }
}

chunk::data::data(size_t total_size, span<const std::string_view> bufs)
  : data(false, total_size) {
  std::byte* pos = storage_;
  for (const auto& buf : bufs) {
    if (!buf.empty()) {
      memcpy(pos, buf.data(), buf.size());
      pos += buf.size();
    }
  }
}

void chunk::data::deref() noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    this->~data();
    free(this);
  }
}

chunk::chunk(const_byte_span buf) {
  init(buf.size(), buf);
}

chunk::chunk(caf::span<const const_byte_span> bufs) {
  auto payload_size = calc_total_size(bufs);
  init(payload_size, payload_size, bufs);
}

template <class... Args>
void chunk::init(size_t payload_size, Args&&... args) {
  auto vptr = malloc(sizeof(data) + payload_size);
  data_.reset(new (vptr) data(std::forward<Args>(args)...), false);
}

} // namespace caf
