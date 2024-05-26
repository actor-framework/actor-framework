// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/basp/header.hpp"

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
