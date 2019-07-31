/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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
namespace net {
namespace basp {

/// @addtogroup BASP

/// Describes the first header field of a BASP message and determines the
/// interpretation of the other header fields.
enum class message_type : uint8_t {
  /// Sends supported BASP version and node information to the server.
  ///
  /// ![](client_handshake.png)
  client_handshake = 0x00,

  /// Sends supported BASP version and node information to the client.
  ///
  /// ![](server_handshake.png)
  server_handshake = 0x01,

  /// Transmits an actor-to-actor messages.
  ///
  /// ![](direct_message.png)
  actor_message = 0x02,

  /// Tries to resolve a path on the receiving node.
  ///
  /// ![](resolve_request.png)
  resolve_request= 0x03,

  /// Transmits the result of a path lookup.
  ///
  /// ![](resolve_response.png)
  resolve_response= 0x04,

  /// Informs the receiving node that the sending node has created a proxy
  /// instance for one of its actors. Causes the receiving node to attach a
  /// functor to the actor that triggers a down_message on termination.
  ///
  /// ![](monitor_message.png)
  monitor_message = 0x05,

  /// Informs the receiving node that it has a proxy for an actor that has been
  /// terminated.
  ///
  /// ![](down_message.png)
  down_message = 0x06,

  /// Used to generate periodic traffic between two nodes in order to detect
  /// disconnects.
  ///
  /// ![](heartbeat.png)
  heartbeat = 0x07,
};

/// @relates message_type
std::string to_string(message_type);

/// @}

} // namespace basp
} // namespace net
} // namespace caf
