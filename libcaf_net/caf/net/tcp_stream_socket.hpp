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
#include "caf/uri.hpp"

namespace caf {
namespace net {

/// Represents a TCP connection. Can be implicitly converted to a
/// `stream_socket` for sending and receiving, or `network_socket`
/// for inspection.
struct tcp_stream_socket : abstract_socket<tcp_stream_socket> {
  using super = abstract_socket<tcp_stream_socket>;

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

/// Create a `stream_socket` connected to `host`:`port`.
/// @param host The remote host to connecto to.
/// @param port The port on the remote host to connect to.
/// @preferred Preferred IP version.
/// @returns The connected socket or an error.
/// @relates stream_socket
expected<stream_socket>
make_connected_socket(ip_address host, uint16_t port);

/// Create a `stream_socket` connected to `auth`.
/// @param authority The remote host to connecto to.
/// @preferred Preferred IP version.
/// @returns The connected socket or an error.
/// @relates stream_socket
expected<stream_socket>
make_connected_socket(const uri::authority_type& auth);

} // namespace net
} // namespace caf
