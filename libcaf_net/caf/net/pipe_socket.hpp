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

#include <cstddef>
#include <system_error>
#include <utility>

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_id.hpp"

// Note: This API mostly wraps platform-specific functions that return ssize_t.
// We return ptrdiff_t instead, since only POSIX defines ssize_t and the two
// types are functionally equivalent.

namespace caf::net {

/// A unidirectional communication endpoint for inter-process communication.
struct CAF_NET_EXPORT pipe_socket : socket {
  using super = socket;

  using super::super;
};

/// Creates two connected sockets. The first socket is the read handle and the
/// second socket is the write handle.
/// @relates pipe_socket
expected<std::pair<pipe_socket, pipe_socket>> CAF_NET_EXPORT make_pipe();

/// Transmits data from `x` to its peer.
/// @param x Connected endpoint.
/// @param buf Memory region for reading the message to send.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates pipe_socket
ptrdiff_t CAF_NET_EXPORT write(pipe_socket x, span<const byte> buf);

/// Receives data from `x`.
/// @param x Connected endpoint.
/// @param buf Memory region for storing the received bytes.
/// @returns The number of received bytes on success, otherwise an error code.
/// @relates pipe_socket
ptrdiff_t CAF_NET_EXPORT read(pipe_socket x, span<byte> buf);

} // namespace caf::net
