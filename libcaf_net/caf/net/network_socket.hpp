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
#include "caf/fwd.hpp"
#include "caf/net/abstract_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_id.hpp"

namespace caf {
namespace net {

struct network_socket : abstract_socket<network_socket> {
  using super = abstract_socket<network_socket>;

  using super::super;

  constexpr operator socket() const noexcept {
    return socket{id};
  }
};

/// Identifies the invalid socket.
/// @relates network_socket
constexpr auto invalid_network_socket = network_socket{invalid_socket_id};

/// Enables or disables keepalive on `x`.
/// @relates network_socket
error keepalive(network_socket x, bool new_value);

/// Enables or disables Nagle's algorithm on `x`.
/// @relates network_socket
error tcp_nodelay(network_socket x, bool new_value);

/// Enables or disables `SIGPIPE` events from `x`.
/// @relates network_socket
error allow_sigpipe(network_socket x, bool new_value);

/// Enables or disables `SIO_UDP_CONNRESET`error on `x`.
/// @relates network_socket
error allow_udp_connreset(network_socket x, bool new_value);

/// Get the socket buffer size for `x`.
/// @pre `x != invalid_network_socket`
/// @relates network_socket
expected<int> send_buffer_size(network_socket x);

/// Set the socket buffer size for `x`.
/// @relates network_socket
error send_buffer_size(network_socket x, int new_value);

/// Returns the locally assigned port of `x`.
/// @relates network_socket
expected<uint16_t> local_port(network_socket x);

/// Returns the locally assigned address of `x`.
/// @relates network_socket
expected<std::string> local_addr(network_socket x);

/// Returns the port used by the remote host of `x`.
/// @relates network_socket
expected<uint16_t> remote_port(network_socket x);

/// Returns the remote host address of `x`.
/// @relates network_socket
expected<std::string> remote_addr(network_socket x);

/// Closes the read channel for a socket.
/// @relates network_socket
void shutdown_read(network_socket x);

/// Closes the write channel for a socket.
/// @relates network_socket
void shutdown_write(network_socket x);

/// Closes the both read and write channel for a socket.
/// @relates network_socket
void shutdown(network_socket x);

/// Transmits data from `x` to its peer.
/// @param x Connected endpoint.
/// @param buf Points to the message to send.
/// @param buf_size Specifies the size of the buffer in bytes.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates pipe_socket
variant<size_t, std::errc> write(network_socket x, const void* buf,
                                 size_t buf_size);

/// Receives data from `x`.
/// @param x Connected endpoint.
/// @param buf Points to destination buffer.
/// @param buf_size Specifies the maximum size of the buffer in bytes.
/// @returns The number of received bytes on success, otherwise an error code.
/// @relates pipe_socket
variant<size_t, std::errc> read(network_socket x, void* buf, size_t buf_size);

} // namespace net
} // namespace caf
