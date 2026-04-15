// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/basp/header.hpp"

#include "caf/detail/network_order.hpp"

#include <cstring>
#include <sstream>

namespace caf::io::basp {

const uint8_t header::named_receiver_flag;

namespace {

template <class T>
bool zero(T val) {
  return val == 0;
}

bool server_handshake_valid(const header& hdr) {
  return !zero(hdr.operation_data);
}

bool client_handshake_valid(const header& hdr) {
  return zero(hdr.source_actor) && zero(hdr.dest_actor);
}

bool direct_message_valid(const header& hdr) {
  return !zero(hdr.dest_actor) && !zero(hdr.payload_len);
}

bool routed_message_valid(const header& hdr) {
  return !zero(hdr.dest_actor) && !zero(hdr.payload_len);
}

bool monitor_message_valid(const header& hdr) {
  return !zero(hdr.payload_len) && zero(hdr.operation_data);
}

bool down_message_valid(const header& hdr) {
  return !zero(hdr.source_actor) && zero(hdr.dest_actor)
         && !zero(hdr.payload_len) && zero(hdr.operation_data);
}

bool heartbeat_valid(const header& hdr) {
  return zero(hdr.source_actor) && zero(hdr.dest_actor) && zero(hdr.payload_len)
         && zero(hdr.operation_data);
}

} // namespace

void header::write_to(std::array<std::byte, header_size>& buf) const noexcept {
  auto* pos = buf.data();
  *pos++ = static_cast<std::byte>(operation);
  *pos++ = static_cast<std::byte>(0); // padding 1
  *pos++ = static_cast<std::byte>(0); // padding 2
  *pos++ = static_cast<std::byte>(flags);
  auto append = [&pos](auto uint_value) {
    auto out_value = detail::to_network_order(uint_value);
    auto bytes = as_bytes(std::span{&out_value, 1});
    memcpy(pos, bytes.data(), bytes.size());
    pos += bytes.size();
  };
  append(payload_len);
  append(operation_data);
  append(source_actor);
  append(dest_actor);
}

bool valid(const header& hdr) {
  switch (hdr.operation) {
    default:
      return false; // invalid operation field
    case message_type::server_handshake:
      return server_handshake_valid(hdr);
    case message_type::client_handshake:
      return client_handshake_valid(hdr);
    case message_type::direct_message:
      return direct_message_valid(hdr);
    case message_type::routed_message:
      return routed_message_valid(hdr);
    case message_type::monitor_message:
      return monitor_message_valid(hdr);
    case message_type::down_message:
      return down_message_valid(hdr);
    case message_type::heartbeat:
      return heartbeat_valid(hdr);
  }
}

} // namespace caf::io::basp
