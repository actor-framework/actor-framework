// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/network_socket.hpp"

#include "caf/detail/net_export.hpp"

#include <cstddef>
#include <variant>

namespace caf::net {

/// A datagram-oriented network communication endpoint.
struct CAF_NET_EXPORT datagram_socket : network_socket {
  using super = network_socket;

  using super::super;
};

/// Enables or disables `SIO_UDP_CONNRESET` error on `x`.
/// @relates datagram_socket
error CAF_NET_EXPORT allow_connreset(datagram_socket x, bool new_value);

/// Converts the result from I/O operation on a ::datagram_socket to either an
/// error code or a integer greater or equal to zero.
/// @relates datagram_socket
std::variant<size_t, sec>
  CAF_NET_EXPORT check_datagram_socket_io_res(std::make_signed_t<size_t> res);

} // namespace caf::net
