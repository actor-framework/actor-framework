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

#include "caf/ip_endpoint.hpp"
#include "caf/net/abstract_socket.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/stream_socket.hpp"
#include "caf/uri.hpp"

namespace caf {
namespace net {

/// Represents a TCP connection.
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

/// Creates a `tcp_stream_socket` connected to given remote node.
/// @param node Host and port of the remote node.
/// @returns The connected socket or an error.
/// @relates tcp_stream_socket
expected<tcp_stream_socket> make_connected_tcp_stream_socket(ip_endpoint node);

/// Create a `tcp_stream_socket` connected to `auth`.
/// @param node Host and port of the remote node.
/// @returns The connected socket or an error.
/// @relates tcp_stream_socket
expected<tcp_stream_socket>
make_connected_tcp_stream_socket(const uri::authority_type& node);

} // namespace net
} // namespace caf
