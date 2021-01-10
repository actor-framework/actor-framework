// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>
#include <type_traits>

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

namespace caf::net::basp {

/// @addtogroup BASP

/// Stores the state of a connection in a `basp::application`.
enum class connection_state : uint8_t {
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
CAF_NET_EXPORT std::string to_string(connection_state x);

/// @relates connection_state
CAF_NET_EXPORT bool from_string(string_view, connection_state&);

/// @relates connection_state
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<connection_state>,
                                 connection_state&);

/// @relates connection_state
template <class Inspector>
bool inspect(Inspector& f, connection_state& x) {
  return default_enum_inspect(f, x);
}

/// @}

} // namespace caf::net::basp
