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

#include "caf/fwd.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/network_socket.hpp"

namespace caf {
namespace net {

/// A datagram-oriented network communication endpoint for bidirectional
/// byte transmission.
struct udp_datagram_socket : abstract_socket<udp_datagram_socket> {
  using super = abstract_socket<udp_datagram_socket>;

  using super::super;

  constexpr operator socket() const noexcept {
    return socket{id};
  }

  constexpr operator network_socket() const noexcept {
    return network_socket{id};
  }
};

/// Creates a `udp_datagram_socket` bound to given port.
/// @param node ip_endpoint that contains the port to bind to. Pass port '0' to
/// bind to any unused port - The endpoint will be updated with the specific
/// port that was bound.
/// @returns The connected socket or an error.
/// @relates udp_datagram_socket
expected<std::pair<udp_datagram_socket, uint16_t>>
make_udp_datagram_socket(ip_endpoint ep, bool reuse_addr = false);

/// Enables or disables `SIO_UDP_CONNRESET` error on `x`.
/// @relates udp_datagram_socket
error allow_connreset(udp_datagram_socket x, bool new_value);

/// Receives the next datagram on socket `x`.
/// @param x The UDP socket for receiving datagrams.
/// @param buf Writable output buffer.
/// @returns The number of received bytes and the sender as `ip_endpoint` on
/// success, an error code otherwise.
/// @relates udp_datagram_socket
/// @post buf was modified and the resulting integer represents the length of
/// the received datagram, even if it did not fit into the given buffer.
variant<std::pair<size_t, ip_endpoint>, sec> read(udp_datagram_socket x,
                                                  span<byte> buf);

/// Sends the content of `buf` as a datagram to the endpoint `ep` on socket `x`.
/// @param x The UDP socket for sending datagrams.
/// @param buf The buffer to send.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates udp_datagram_socket
variant<size_t, sec> write(udp_datagram_socket x, span<const byte> buf,
                           ip_endpoint ep);

/// Converts the result from I/O operation on a ::udp_datagram_socket to either
/// an error code or a non-zero positive integer.
/// @relates udp_datagram_socket
variant<size_t, sec>
check_udp_datagram_socket_io_res(std::make_signed<size_t>::type res);

} // namespace net
} // namespace caf
