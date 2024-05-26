// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket.hpp"
#include "caf/net/socket_id.hpp"

#include "caf/config.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

namespace caf::net {

/// A bidirectional network communication endpoint.
struct CAF_NET_EXPORT network_socket : socket {
  using super = socket;

  using super::super;
};

/// Enables or disables `SIGPIPE` events from `x`.
/// @relates network_socket
error CAF_NET_EXPORT allow_sigpipe(network_socket x, bool new_value);

/// Enables or disables `SIO_UDP_CONNRESET`error on `x`.
/// @relates network_socket
error CAF_NET_EXPORT allow_udp_connreset(network_socket x, bool new_value);

/// Get the socket buffer size for `x`.
/// @pre `x != invalid_socket`
/// @relates network_socket
expected<size_t> CAF_NET_EXPORT send_buffer_size(network_socket x);

/// Set the socket buffer size for `x`.
/// @relates network_socket
error CAF_NET_EXPORT send_buffer_size(network_socket x, size_t capacity);

/// Returns the locally assigned port of `x`.
/// @relates network_socket
expected<uint16_t> CAF_NET_EXPORT local_port(network_socket x);

/// Returns the locally assigned address of `x`.
/// @relates network_socket
expected<std::string> CAF_NET_EXPORT local_addr(network_socket x);

/// Returns the port used by the remote host of `x`.
/// @relates network_socket
expected<uint16_t> CAF_NET_EXPORT remote_port(network_socket x);

/// Returns the remote host address of `x`.
/// @relates network_socket
expected<std::string> CAF_NET_EXPORT remote_addr(network_socket x);

/// Closes the read channel for a socket.
/// @relates network_socket
void CAF_NET_EXPORT shutdown_read(network_socket x);

/// Closes the write channel for a socket.
/// @relates network_socket
void CAF_NET_EXPORT shutdown_write(network_socket x);

/// Closes the both read and write channel for a socket.
/// @relates network_socket
void CAF_NET_EXPORT shutdown(network_socket x);

} // namespace caf::net
