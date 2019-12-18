/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
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

#include <cstdint>
#include <string>
#include <system_error>
#include <type_traits>

#include "caf/config.hpp"
#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_id.hpp"

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
