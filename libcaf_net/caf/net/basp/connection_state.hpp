// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <string>

namespace caf::net::basp {

/// @addtogroup BASP

/// Stores the state of a connection in a `basp::application`.
enum class connection_state {
  /// Initial state for any connection to wait for the peer's handshake.
  await_handshake_header,
  /// Indicates that the header for the peer's handshake arrived and BASP
  /// requires the payload next.
  await_handshake_payload,
  /// Indicates that a connection is established and this node is waiting for
  /// the next BASP header.
  await_header,
  /// Indicates that this node has received a header with non-zero payload and
  /// is waiting for the data.
  await_payload,
  /// Indicates that the connection is about to shut down.
  shutdown,
};

/// @relates connection_state
std::string to_string(connection_state x);

/// @}

} // namespace caf::net::basp
