// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/net/socket.hpp"
#include "caf/net/socket_id.hpp"

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"

#include <cstddef>
#include <system_error>
#include <utility>

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
ptrdiff_t CAF_NET_EXPORT write(pipe_socket x, const_byte_span buf);

/// Receives data from `x`.
/// @param x Connected endpoint.
/// @param buf Memory region for storing the received bytes.
/// @returns The number of received bytes on success, otherwise an error code.
/// @relates pipe_socket
ptrdiff_t CAF_NET_EXPORT read(pipe_socket x, byte_span buf);

} // namespace caf::net
