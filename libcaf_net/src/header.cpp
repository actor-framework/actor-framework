// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/basp/header.hpp"

#include <cstring>

#include "caf/byte.hpp"
#include "caf/byte_buffer.hpp"
#include "caf/detail/network_order.hpp"
#include "caf/span.hpp"

namespace caf::net::basp {

namespace {

void to_bytes_impl(const header& x, byte* ptr) {
  *ptr = static_cast<byte>(x.type);
  auto payload_len = detail::to_network_order(x.payload_len);
  memcpy(ptr + 1, &payload_len, sizeof(payload_len));
  auto operation_data = detail::to_network_order(x.operation_data);
  memcpy(ptr + 5, &operation_data, sizeof(operation_data));
}

} // namespace

int header::compare(header other) const noexcept {
  auto x = to_bytes(*this);
  auto y = to_bytes(other);
  return memcmp(x.data(), y.data(), header_size);
}

header header::from_bytes(span<const byte> bytes) {
  CAF_ASSERT(bytes.size() >= header_size);
  header result;
  auto ptr = bytes.data();
  result.type = *reinterpret_cast<const message_type*>(ptr);
  uint32_t payload_len = 0;
  memcpy(&payload_len, ptr + 1, 4);
  result.payload_len = detail::from_network_order(payload_len);
  uint64_t operation_data;
  memcpy(&operation_data, ptr + 5, 8);
  result.operation_data = detail::from_network_order(operation_data);
  return result;
}

std::array<byte, header_size> to_bytes(header x) {
  std::array<byte, header_size> result{};
  to_bytes_impl(x, result.data());
  return result;
}

void to_bytes(header x, byte_buffer& buf) {
  buf.resize(header_size);
  to_bytes_impl(x, buf.data());
}

} // namespace caf::net::basp
