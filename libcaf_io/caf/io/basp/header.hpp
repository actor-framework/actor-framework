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

#pragma once

#include <string>
#include <cstdint>

#include "caf/error.hpp"
#include "caf/node_id.hpp"

#include "caf/meta/omittable.hpp"
#include "caf/meta/type_name.hpp"

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
  uint8_t padding1;
  uint8_t padding2;
  uint8_t flags;
  uint32_t payload_len;
  uint64_t operation_data;
  actor_id source_actor;
  actor_id dest_actor;

  header(message_type m_operation, uint8_t m_flags, uint32_t m_payload_len,
         uint64_t m_operation_data, actor_id m_source_actor,
         actor_id m_dest_actor)
      : operation(m_operation),
        flags(m_flags),
        payload_len(m_payload_len),
        operation_data(m_operation_data),
        source_actor(m_source_actor),
        dest_actor(m_dest_actor) {
    // nop
  }

  header() = default;

  /// Identifies a receiver by name rather than ID.
  static const uint8_t named_receiver_flag = 0x01;

  /// Queries whether this header has the given flag.
  bool has(uint8_t flag) const {
    return (flags & flag) != 0;
  }
};

/// @relates header
template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, header& hdr) {
  uint8_t pad = 0;
  return f(meta::type_name("header"),
           hdr.operation,
           meta::omittable(), pad,
           meta::omittable(), pad,
           hdr.flags, hdr.payload_len, hdr.operation_data,
           hdr.source_actor, hdr.dest_actor);
}

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
constexpr size_t header_size = sizeof(actor_id) * 2
                               + sizeof(uint32_t) * 2
                               + sizeof(uint64_t);

/// @}

} // namespace basp
} // namespace io
} // namespace caf

