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

#include "caf/net/abstract_socket.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/uri.hpp"

namespace caf {
namespace net {

/// Represents an open TCP endpoint. Can be implicitly converted to a
/// `stream_socket` for sending and receiving, or `network_socket`
/// for inspection.
struct tcp_accept_socket : abstract_socket<tcp_accept_socket> {
  using super = abstract_socket<tcp_accept_socket>;

  using super::super;

  constexpr operator socket() const noexcept {
    return socket{id};
  }

  constexpr operator network_socket() const noexcept {
    return network_socket{id};
  }

  constexpr operator stream_socket() const noexcept {
    return stream_socket{id};
  }
};

/// Creates a new TCP socket to accept connections on a given port.
/// @param port The port to listen on.
/// @param addr Only accepts connections originating from this address.
/// @param reuse_addr Optionally sets the SO_REUSEADDR option on the socket.
/// @relates stream_socket
expected<tcp_accept_socket> make_accept_socket(ip_address host, uint16_t port,
                                               bool reuse_addr);

/// Creates a new TCP socket to accept connections on a given port.
/// @param port The port to listen on.
/// @param addr Only accepts connections originating from this address.
/// @param reuse_addr Optionally sets the SO_REUSEADDR option on the socket.
/// @relates stream_socket
expected<tcp_accept_socket> make_accept_socket(const uri::authority_type& auth,
                                               bool reuse_addr = false);

/// Accept a connection on `x`.
/// @param x Listening endpoint.
/// @returns The socket that handles the accepted connection.
/// @relates stream_socket
expected<tcp_stream_socket> accept(tcp_accept_socket x);

} // namespace net
} // namespace caf
