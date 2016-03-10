/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_IO_BASP_HEADER_HPP
#define CAF_IO_BASP_HEADER_HPP

#include <string>
#include <cstdint>

#include "caf/node_id.hpp"

#include "caf/io/basp/message_type.hpp"

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

/// The header of a Binary Actor System Protocol (BASP) message.
/// A BASP header consists of a routing part, i.e., source and
/// destination, as well as an operation and operation data. Several
/// message types consist of only a header.
struct header {
  message_type operation;
  uint32_t payload_len;
  uint64_t operation_data;
  node_id source_node;
  node_id dest_node;
  actor_id source_actor;
  actor_id dest_actor;
};

/// @relates header
template <class T>
void serialize(T& in_or_out, header& hdr, const unsigned int) {
  in_or_out & hdr.source_node;
  in_or_out & hdr.dest_node;
  in_or_out & hdr.source_actor;
  in_or_out & hdr.dest_actor;
  in_or_out & hdr.payload_len;
  in_or_out & hdr.operation;
  in_or_out & hdr.operation_data;
}

/// @relates header
std::string to_string(const header& hdr);

/// @relates header
bool operator==(const header& lhs, const header& rhs);

/// @relates header
inline bool operator!=(const header& lhs, const header& rhs) {
  return !(lhs == rhs);
}

/// Checks whether given header contains a handshake.
inline bool is_handshake(const header& hdr) {
  return hdr.operation == message_type::server_handshake
      || hdr.operation == message_type::client_handshake;
}

/// Checks wheter given header contains a heartbeat.
inline bool is_heartbeat(const header& hdr) {
  return hdr.operation == message_type::heartbeat;
}

/// Checks whether given BASP header is valid.
/// @relates header
bool valid(const header& hdr);

/// Size of a BASP header in serialized form
constexpr size_t header_size = node_id::serialized_size * 2
                               + sizeof(actor_id) * 2
                               + sizeof(uint32_t) * 2
                               + sizeof(uint64_t);

/// @}

} // namespace basp
} // namespace io
} // namespace caf

#endif // CAF_IO_BASP_HEADER_HPP
