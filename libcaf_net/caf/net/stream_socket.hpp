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

#include "caf/expected.hpp"
#include "caf/fwd.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/sec.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

/// A connection-oriented network communication endpoint for bidirectional byte
/// streams.
struct stream_socket : abstract_socket<stream_socket> {
  using super = abstract_socket<stream_socket>;

  using super::super;

  constexpr operator socket() const noexcept {
    return socket{id};
  }

  constexpr operator network_socket() const noexcept {
    return network_socket{id};
  }
};

/// Creates two connected sockets to mimic network communication (usually for
/// testing purposes).
/// @relates stream_socket
expected<std::pair<stream_socket, stream_socket>> make_stream_socket_pair();

/// Enables or disables keepalive on `x`.
/// @relates network_socket
error keepalive(stream_socket x, bool new_value);

/// Enables or disables Nagle's algorithm on `x`.
/// @relates stream_socket
error nodelay(stream_socket x, bool new_value);

/// Receives data from `x`.
/// @param x Connected endpoint.
/// @param buf Points to destination buffer.
/// @param buf_size Specifies the maximum size of the buffer in bytes.
/// @returns The number of received bytes on success, an error code otherwise.
/// @relates stream_socket
/// @post either the result is a `sec` or a positive (non-zero) integer
variant<size_t, sec> read(stream_socket x, span<byte> buf);

/// Transmits data from `x` to its peer.
/// @param x Connected endpoint.
/// @param buf Points to the message to send.
/// @param buf_size Specifies the size of the buffer in bytes.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates stream_socket
/// @post either the result is a `sec` or a positive (non-zero) integer
variant<size_t, sec> write(stream_socket x, span<const byte> buf);

/// Converts the result from I/O operation on a ::stream_socket to either an
/// error code or a non-zero positive integer.
/// @relates stream_socket
variant<size_t, sec>
check_stream_socket_io_res(std::make_signed<size_t>::type res);

} // namespace net
} // namespace caf
