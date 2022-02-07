// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/net_export.hpp"
#include "caf/ip_endpoint.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/uri.hpp"

namespace caf::net {

/// Represents a TCP connection.
struct CAF_NET_EXPORT tcp_stream_socket : stream_socket {
  using super = stream_socket;

  using super::super;
};

/// Creates a `tcp_stream_socket` connected to given remote node.
/// @param node Host and port of the remote node.
/// @returns The connected socket or an error.
/// @relates tcp_stream_socket
expected<tcp_stream_socket>
  CAF_NET_EXPORT make_connected_tcp_stream_socket(ip_endpoint node);

/// Creates a `tcp_stream_socket` connected to @p auth.
/// @param auth Host and port of the remote node.
/// @returns The connected socket or an error.
/// @relates tcp_stream_socket
expected<tcp_stream_socket> CAF_NET_EXPORT
make_connected_tcp_stream_socket(const uri::authority_type& auth);

} // namespace caf::net
