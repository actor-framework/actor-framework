// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace caf::net::web_socket {

/// Status codes as defined by RFC 6455, Section 7.4.
enum class status : uint16_t {
  /// Indicates a normal closure, meaning that the purpose for which the
  /// connection was established has been fulfilled.
  normal_close = 1000,

  /// Indicates that an endpoint is "going away", such as a server going down or
  /// a browser having navigated away from a page.
  going_away = 1001,

  /// Indicates that an endpoint is terminating the connection due to a protocol
  /// error.
  protocol_error = 1002,

  /// Indicates that an endpoint is terminating the connection because it has
  /// received a type of data it cannot accept (e.g., an endpoint that
  /// understands only text data MAY send this if it receives a binary message).
  invalid_data = 1003,

  /// A reserved value and MUST NOT be set as a status code in a Close control
  /// frame by an endpoint.  It is designated for use in applications expecting
  /// a status code to indicate that no status code was actually present.
  no_status = 1005,

  /// A reserved value and MUST NOT be set as a status code in a Close control
  /// frame by an endpoint.  It is designated for use in applications expecting
  /// a status code to indicate that the connection was closed abnormally, e.g.,
  /// without sending or receiving a Close control frame.
  abnormal_exit = 1006,

  /// Indicates that an endpoint is terminating the connection because it has
  /// received data within a message that was not consistent with the type of
  /// the message (e.g., non-UTF-8 [RFC3629] data within a text message).
  inconsistent_data = 1007,

  /// Indicates that an endpoint is terminating the connection because it has
  /// received a message that violates its policy.  This is a generic status
  /// code that can be returned when there is no other more suitable status code
  /// (e.g., 1003 or 1009) or if there is a need to hide specific details about
  /// the policy.
  policy_violation = 1008,

  /// Indicates that an endpoint is terminating the connection because it has
  /// received a message that is too big for it to process.
  message_too_big = 1009,

  /// Indicates that an endpoint (client) is terminating the connection because
  /// it has expected the server to negotiate one or more extension, but the
  /// server didn't return them in the response message of the WebSocket
  /// handshake. The list of extensions that are needed SHOULD appear in the
  /// /reason/ part of the Close frame. Note that this status code is not used
  /// by the server, because it can fail the WebSocket handshake instead.
  missing_extensions = 1010,

  /// Indicates that a server is terminating the connection because it
  /// encountered an unexpected condition that prevented it from fulfilling the
  /// request.
  unexpected_condition = 1011,

  /// A reserved value and MUST NOT be set as a status code in a Close control
  /// frame by an endpoint.  It is designated for use in applications expecting
  /// a status code to indicate that the connection was closed due to a failure
  /// to perform a TLS handshake (e.g., the server certificate can't be
  /// verified).
  tls_handshake_failure = 1015,
};

/// @relates status
CAF_NET_EXPORT std::string to_string(status);

/// @relates status
CAF_NET_EXPORT bool from_string(std::string_view, status&);

/// @relates status
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<status>, status&);

/// @relates status
template <class Inspector>
bool inspect(Inspector& f, status& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::web_socket
