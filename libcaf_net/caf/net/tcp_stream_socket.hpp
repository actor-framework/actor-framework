// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/timespan.hpp"
#include "caf/uri.hpp"

namespace caf::net {

/// Represents a TCP connection.
struct CAF_NET_EXPORT tcp_stream_socket : stream_socket {
  using super = stream_socket;

  using super::super;
};

/// Creates a `tcp_stream_socket` connected to given remote node.
/// @param node Host and port of the remote node.
/// @param timeout Maximum waiting time on the connection before canceling it.
/// @returns The connected socket or an error.
/// @relates tcp_stream_socket
expected<tcp_stream_socket> CAF_NET_EXPORT //
make_connected_tcp_stream_socket(ip_endpoint node, timespan timeout = infinite);

/// Creates a `tcp_stream_socket` connected to @p auth.
/// @param auth Host and port of the remote node.
/// @param timeout Maximum waiting time on the connection before canceling it.
/// @returns The connected socket or an error.
/// @note The timeout applies to a *single* connection attempt. If the DNS
///       lookup for @p auth returns more than one possible IP address then the
///       @p timeout applies to each connection attempt individually. For
///       example, passing a timeout of one second with a DNS result of five
///       entries would mean this function can block up to five seconds if all
///       attempts time out.
/// @relates tcp_stream_socket
expected<tcp_stream_socket> CAF_NET_EXPORT //
make_connected_tcp_stream_socket(const uri::authority_type& auth,
                                 timespan timeout = infinite);

/// Creates a `tcp_stream_socket` connected to given @p host and @p port.
/// @param host TCP endpoint for connecting to.
/// @param port TCP port of the server.
/// @param timeout Maximum waiting time on the connection before canceling it.
/// @returns The connected socket or an error.
/// @note The timeout applies to a *single* connection attempt. If the DNS
///       lookup for @p auth returns more than one possible IP address then the
///       @p timeout applies to each connection attempt individually. For
///       example, passing a timeout of one second with a DNS result of five
///       entries would mean this function can block up to five seconds if all
///       attempts time out.
/// @relates tcp_stream_socket
expected<tcp_stream_socket> CAF_NET_EXPORT //
make_connected_tcp_stream_socket(std::string host, uint16_t port,
                                 timespan timeout = infinite);

} // namespace caf::net

namespace caf::detail {

expected<net::tcp_stream_socket> CAF_NET_EXPORT //
tcp_try_connect(std::string host, uint16_t port, timespan connection_timeout,
                size_t max_retry_count, timespan retry_delay);

} // namespace caf::detail
