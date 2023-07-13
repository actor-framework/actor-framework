// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/basp/header.hpp"

#include <sstream>

namespace caf::io::basp {

const uint8_t header::named_receiver_flag;

std::string to_bin(uint8_t x) {
  std::string res;
  for (auto offset = 7; offset > -1; --offset)
    res += std::to_string((x >> offset) & 0x01);
  return res;
}

std::string to_string(const header& hdr) {
  std::ostringstream oss;
  oss << "{" << to_string(hdr.operation) << ", " << to_bin(hdr.flags) << ", "
      << hdr.payload_len << ", " << hdr.operation_data << ", "
      << hdr.source_actor << ", " << hdr.dest_actor << "}";
  return oss.str();
}

bool operator==(const header& lhs, const header& rhs) {
  return lhs.operation == rhs.operation && lhs.flags == rhs.flags
         && lhs.payload_len == rhs.payload_len
         && lhs.operation_data == rhs.operation_data
         && lhs.source_actor == rhs.source_actor
         && lhs.dest_actor == rhs.dest_actor;
}

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
