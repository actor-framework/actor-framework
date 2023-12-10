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

chunk::data* chunk::data::make(bool is_binary, size_t payload_size) {
  auto vptr = malloc(sizeof(data) + payload_size);
  if (vptr == nullptr)
    CAF_RAISE_ERROR(std::bad_alloc, "chunk::data::make");
  return new (vptr) data(is_binary, payload_size);
}

chunk::data* chunk::data::make(const_byte_span buffer) {
  auto* result = make(true, buffer.size());
  memcpy(result->storage(), buffer.data(), buffer.size());
  return result;
}

chunk::data* chunk::data::make(std::string_view text) {
  auto* result = make(false, text.size());
  memcpy(result->storage(), text.data(), text.size());
  return result;
}

chunk::data* chunk::data::make(span<const const_byte_span> buffers) {
  auto* result = make(true, calc_total_size(buffers));
  auto pos = result->storage();
  for (const auto& buffer : buffers) {
    if (!buffer.empty()) {
      memcpy(pos, buffer.data(), buffer.size());
      pos += buffer.size();
    }
  }
  return result;
}

chunk::data* chunk::data::make(span<const std::string_view> texts) {
  auto* result = make(false, calc_total_size(texts));
  auto pos = result->storage();
  for (const auto& text : texts) {
    if (!text.empty()) {
      memcpy(pos, text.data(), text.size());
      pos += text.size();
    }
  }
  return result;
}

void chunk::data::deref() noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    this->~data();
    free(this);
  }
}

} // namespace caf
