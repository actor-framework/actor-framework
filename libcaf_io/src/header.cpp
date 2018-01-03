/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/io/basp/header.hpp"

#include <sstream>

namespace caf {
namespace io {
namespace basp {

const uint8_t header::named_receiver_flag;

std::string to_bin(uint8_t x) {
  std::string res;
  for (auto offset = 7; offset > -1; --offset)
    res += std::to_string((x >> offset) & 0x01);
  return res;
}

std::string to_string(const header &hdr) {
  std::ostringstream oss;
  oss << "{"
      << to_string(hdr.operation) << ", "
      << to_bin(hdr.flags) << ", "
      << hdr.payload_len << ", "
      << hdr.operation_data << ", "
      << to_string(hdr.source_node) << ", "
      << to_string(hdr.dest_node) << ", "
      << hdr.source_actor << ", "
      << hdr.dest_actor << ", "
      << hdr.sequence_number
      << "}";
  return oss.str();
}

bool operator==(const header& lhs, const header& rhs) {
  return lhs.operation == rhs.operation
      && lhs.flags == rhs.flags
      && lhs.payload_len == rhs.payload_len
      && lhs.operation_data == rhs.operation_data
      && lhs.source_node == rhs.source_node
      && lhs.dest_node == rhs.dest_node
      && lhs.source_actor == rhs.source_actor
      && lhs.dest_actor == rhs.dest_actor
      && lhs.sequence_number == rhs.sequence_number;
}

namespace {

bool valid(const node_id& val) {
  return val != none;
}

template <class T>
bool zero(T val) {
  return val == 0;
}

bool server_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && zero(hdr.dest_actor)
       && !zero(hdr.operation_data);
}

bool client_handshake_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor);
}

bool dispatch_message_valid(const header& hdr) {
  return  valid(hdr.dest_node)
       && (!zero(hdr.dest_actor) || hdr.has(header::named_receiver_flag))
       && !zero(hdr.payload_len);
}

bool announce_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && !zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

bool kill_proxy_instance_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && !zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && !zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

bool heartbeat_valid(const header& hdr) {
  return  valid(hdr.source_node)
       && valid(hdr.dest_node)
       && hdr.source_node != hdr.dest_node
       && zero(hdr.source_actor)
       && zero(hdr.dest_actor)
       && zero(hdr.payload_len)
       && zero(hdr.operation_data);
}

} // namespace <anonymous>

bool valid(const header& hdr) {
  switch (hdr.operation) {
    default:
      return false; // invalid operation field
    case message_type::server_handshake:
      return server_handshake_valid(hdr);
    case message_type::client_handshake:
      return client_handshake_valid(hdr);
    case message_type::dispatch_message:
      return dispatch_message_valid(hdr);
    case message_type::announce_proxy:
      return announce_proxy_instance_valid(hdr);
    case message_type::kill_proxy:
      return kill_proxy_instance_valid(hdr);
    case message_type::heartbeat:
      return heartbeat_valid(hdr);
  }
}

} // namespace basp
} // namespace io
} // namespace caf
