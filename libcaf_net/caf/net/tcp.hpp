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

#include <cstdint>

#include "caf/expected.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/stream_socket.hpp"

namespace caf {
namespace net {

struct tcp {

/// Creates a new TCP socket to accept connections on a given port.
/// @param port The port to listen on.
/// @param addr Only accepts connections originating from this address.
/// @param reuse_addr Optionally sets the SO_REUSEADDR option on the socket.
/// @relates stream_socket
static expected<stream_socket> make_accept_socket(uint16_t port,
                                                  const char* addr = nullptr,
                                                  bool reuse_addr = false);
/// Create a `stream_socket` connected to `host`:`port` via the
/// `preferred` IP version.
/// @param host The remote host to connecto to.
/// @param port The port on the remote host to connect to.
/// @preferred Preferred IP version.
/// @returns The connected socket or an error.
/// @relates stream_socket
static expected<stream_socket> make_connected_socket(std::string host,
                                                     uint16_t port,
                                                     optional<ip> preferred = none);

/// Accept a connection on `x`.
/// @param x Listening endpoint.
/// @returns The socket that handles the accepted connection.
/// @relates stream_socket
static expected<stream_socket> accept(stream_socket x);

};

} // namespace net
} // namespace caf
