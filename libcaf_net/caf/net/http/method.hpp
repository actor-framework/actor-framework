// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/net_export.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace caf::net::http {

/// Methods as defined by RFC 7231.
enum class method : uint8_t {
  /// Requests transfer of a current selected representation for the target
  /// resource.
  get,
  /// Identical to GET except that the server MUST NOT send a message body in
  /// the response. The server SHOULD send the same header fields in response to
  /// a HEAD request as it would have sent if the request had been a GET, except
  /// that the payload header fields (Section 3.3) MAY be omitted.
  head,
  /// Requests that the target resource process the representation enclosed in
  /// the request according to the resource's own specific semantics.
  post,
  /// Requests that the state of the target resource be created or replaced with
  /// the state defined by the representation enclosed in the request message
  /// payload.
  put,
  /// Requests that the origin server remove the association between the target
  /// resource and its current functionality.
  del,
  /// Requests that the recipient establish a tunnel to the destination origin
  /// server identified by the request-target and, if successful, thereafter
  /// restrict its behavior to blind forwarding of packets, in both directions,
  /// until the tunnel is closed.
  connect,
  /// Requests information about the communication options available for the
  /// target resource, at either the origin server or an intervening
  /// intermediary.
  options,
  /// Requests a remote, application-level loop-back of the request message.
  trace,
};

/// @relates method
CAF_NET_EXPORT std::string to_string(method);

/// Converts @p x to the RFC string representation, i.e., all-uppercase.
/// @relates method
CAF_NET_EXPORT std::string_view to_rfc_string(method x);

/// @relates method
CAF_NET_EXPORT bool from_string(std::string_view, method&);

/// @relates method
CAF_NET_EXPORT bool from_integer(std::underlying_type_t<method>, method&);

/// @relates method
template <class Inspector>
bool inspect(Inspector& f, method& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::net::http
