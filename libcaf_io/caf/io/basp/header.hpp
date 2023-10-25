// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/io/basp/message_type.hpp"

#include "caf/detail/io_export.hpp"
#include "caf/error.hpp"
#include "caf/node_id.hpp"

#include <cstdint>
#include <string>

namespace caf::io::basp {

/// @addtogroup BASP
/// @{

/// The header of a Binary Actor System Protocol (BASP) message. A BASP header
/// consists of a routing part, i.e., source and destination, as well as an
/// operation and operation data. Several message types consist of only a
/// header.
struct header {
  message_type operation;
  uint8_t padding1 = 0;
  uint8_t padding2 = 0;
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

  /// Identifies the config server.
  static const uint64_t config_server_id = 1;

  /// Identifies the spawn server.
  static const uint64_t spawn_server_id = 2;

  /// Queries whether this header has the given flag.
  bool has(uint8_t flag) const {
    return (flags & flag) != 0;
  }
};

/// @relates header
template <class Inspector>
bool inspect(Inspector& f, header& x) {
  uint8_t pad = 0;
  return f.object(x).fields(f.field("operation", x.operation),
                            f.field("pad1", pad), f.field("pad2", pad),
                            f.field("flags", x.flags),
                            f.field("payload_len", x.payload_len),
                            f.field("operation_data", x.operation_data),
                            f.field("source_actor", x.source_actor),
                            f.field("dest_actor", x.dest_actor));
}

/// Checks whether given header contains a handshake.
inline bool is_handshake(const header& hdr) {
  return hdr.operation == message_type::server_handshake
         || hdr.operation == message_type::client_handshake;
}

/// Checks whether given header contains a heartbeat.
inline bool is_heartbeat(const header& hdr) {
  return hdr.operation == message_type::heartbeat;
}

/// Checks whether given BASP header is valid.
/// @relates header
CAF_IO_EXPORT bool valid(const header& hdr);

/// Size of a BASP header in serialized form
/// @relates header
constexpr size_t header_size = sizeof(actor_id) * 2 + sizeof(uint32_t) * 2
                               + sizeof(uint64_t);

/// @}

} // namespace caf::io::basp
