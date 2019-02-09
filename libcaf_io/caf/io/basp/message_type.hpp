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

namespace caf {
namespace io {
namespace basp {

/// @addtogroup BASP

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

/// @relates message_type
std::string to_string(message_type);

/// @}

} // namespace basp
} // namespace io
} // namespace caf
