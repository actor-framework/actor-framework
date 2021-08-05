// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/io_export.hpp"

namespace caf::io::basp {

/// @addtogroup BASP
/// @{

/// Describes the first header field of a BASP message and determines the
/// interpretation of the other header fields.
enum class message_type : uint8_t {
  /// Send from server, i.e., the node with a published actor, to client,
  /// i.e., node that initiates a new connection using remote_actor().
  ///
  /// ![](server_handshake.png)
  server_handshake = 0x00,

  /// Send from client to server after it has successfully received the
  /// server_handshake to establish the connection.
  ///
  /// ![](client_handshake.png)
  client_handshake = 0x01,

  /// Transmits a direct message from source to destination.
  ///
  /// ![](direct_message.png)
  direct_message = 0x02,

  /// Transmits a message from `source_node:source_actor` to
  /// `dest_node:dest_actor`.
  ///
  /// ![](routed_message.png)
  routed_message = 0x03,

  /// Informs the receiving node that the sending node has created a proxy
  /// instance for one of its actors. Causes the receiving node to attach
  /// a functor to the actor that triggers a down_message on termination.
  ///
  /// ![](monitor_message.png)
  monitor_message = 0x04,

  /// Informs the receiving node that it has a proxy for an actor
  /// that has been terminated.
  ///
  /// ![](down_message.png)
  down_message = 0x05,

  /// Used to generate periodic traffic between two nodes
  /// in order to detect disconnects.
  ///
  /// ![](heartbeat.png)
  heartbeat = 0x06,
};

CAF_IO_EXPORT std::string to_string(message_type);

CAF_IO_EXPORT bool from_string(std::string_view, message_type&);

CAF_IO_EXPORT bool from_integer(std::underlying_type_t<message_type>,
                                message_type&);

template <class Inspector>
bool inspect(Inspector& f, message_type& x) {
  return default_enum_inspect(f, x);
}

/// @}

} // namespace caf::io::basp
