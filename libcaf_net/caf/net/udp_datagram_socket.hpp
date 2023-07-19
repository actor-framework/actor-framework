// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/net/network_socket.hpp"

#include "caf//net/datagram_socket.hpp"
#include "caf/byte_span.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <vector>

namespace caf::net {

/// A datagram-oriented network communication endpoint for bidirectional
/// byte transmission.
struct CAF_NET_EXPORT udp_datagram_socket : datagram_socket {
  using super = datagram_socket;

  using super::super;
};

/// Creates a `udp_datagram_socket` bound to given port.
/// @param ep ip_endpoint that contains the port to bind to. Pass port '0' to
///           bind to any unused port - The endpoint will be updated with the
///           specific port that was bound.
/// @returns The connected socket or an error.
/// @relates udp_datagram_socket
expected<udp_datagram_socket>
  CAF_NET_EXPORT make_udp_datagram_socket(ip_endpoint ep,
                                          bool reuse_addr = true);

/// Receives the next datagram on socket `x`.
/// @param x The UDP socket for receiving datagrams.
/// @param buf Writable output buffer.
/// @param src If non-null, CAF optionally stores the address of the sender for
///            the datagram.
/// @returns The number of received bytes on success, 0 if the socket is closed,
///          or -1 in case of an error.
/// @relates udp_datagram_socket
/// @post buf was modified and the resulting integer represents the length of
///       the received datagram, even if it did not fit into the given buffer.
ptrdiff_t CAF_NET_EXPORT read(udp_datagram_socket x, byte_span buf,
                              ip_endpoint* src = nullptr);

/// Sends the content of `buf` as a datagram to the endpoint `ep` on socket `x`.
/// @param x The UDP socket for sending datagrams.
/// @param buf The buffer to send.
/// @param ep The endpoint to send the datagram to.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates udp_datagram_socket
ptrdiff_t CAF_NET_EXPORT write(udp_datagram_socket x, const_byte_span buf,
                               ip_endpoint ep);

} // namespace caf::net
