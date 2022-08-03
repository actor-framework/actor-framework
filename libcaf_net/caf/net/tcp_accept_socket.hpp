// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/fwd.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/uri.hpp"

namespace caf::net {

/// Represents a TCP acceptor in listening mode.
struct CAF_NET_EXPORT tcp_accept_socket : network_socket {
  using super = network_socket;

  using super::super;
};

/// Creates a new TCP socket to accept connections on a given port.
/// @param node The endpoint to listen on and the filter for incoming addresses.
///             Passing the address `0.0.0.0` will accept incoming connection
///             from any host. Passing port 0 lets the OS choose the port.
/// @relates tcp_accept_socket
expected<tcp_accept_socket>
  CAF_NET_EXPORT make_tcp_accept_socket(ip_endpoint node,
                                        bool reuse_addr = true);

/// Creates a new TCP socket to accept connections on a given port.
/// @param node The endpoint to listen on and the filter for incoming addresses.
///             Passing the address `0.0.0.0` will accept incoming connection
///             from any host. Passing port 0 lets the OS choose the port.
/// @param reuse_addr Optionally sets the SO_REUSEADDR option on the socket.
/// @relates tcp_accept_socket
expected<tcp_accept_socket>
  CAF_NET_EXPORT make_tcp_accept_socket(const uri::authority_type& node,
                                        bool reuse_addr = true);

/// Creates a new TCP socket to accept connections on a given port.
/// @param port The port for listening to incoming connection. Passing 0 lets
///             the OS choose a port.
/// @param addr The filter for incoming addresses. Passing the address `0.0.0.0`
///             will accept incoming connection from any host.
/// @param reuse_addr Optionally sets the SO_REUSEADDR option on the socket.
/// @relates tcp_accept_socket
expected<tcp_accept_socket> CAF_NET_EXPORT make_tcp_accept_socket(
  uint16_t port, std::string addr = "0.0.0.0", bool reuse_addr = true);

/// Accepts a connection on `x`.
/// @param x Listening endpoint.
/// @returns The socket that handles the accepted connection on success, an
/// error otherwise.
/// @relates tcp_accept_socket
expected<tcp_stream_socket> CAF_NET_EXPORT accept(tcp_accept_socket x);

} // namespace caf::net
