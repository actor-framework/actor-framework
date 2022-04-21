// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>

#include "caf/detail/net_export.hpp"
#include "caf/fwd.hpp"
#include "caf/net/network_socket.hpp"

// Note: This API mostly wraps platform-specific functions that return ssize_t.
// We return ptrdiff_t instead, since only POSIX defines ssize_t and the two
// types are functionally equivalent.

namespace caf::net {

/// A connection-oriented network communication endpoint for bidirectional byte
/// streams.
struct CAF_NET_EXPORT stream_socket : network_socket {
  using super = network_socket;

  using super::super;
};

/// Creates two connected sockets to mimic network communication (usually for
/// testing purposes).
/// @relates stream_socket
expected<std::pair<stream_socket, stream_socket>>
  CAF_NET_EXPORT make_stream_socket_pair();

/// Enables or disables keepalive on `x`.
/// @relates network_socket
error CAF_NET_EXPORT keepalive(stream_socket x, bool new_value);

/// Enables or disables Nagle's algorithm on `x`.
/// @relates stream_socket
error CAF_NET_EXPORT nodelay(stream_socket x, bool new_value);

/// Receives data from `x`.
/// @param x A connected endpoint.
/// @param buf Points to destination buffer.
/// @returns The number of received bytes on success, 0 if the socket is closed,
///          or -1 in case of an error.
/// @relates stream_socket
/// @post Either the functions returned a non-negative integer or the caller can
///       retrieve the error code by calling `last_socket_error()`.
ptrdiff_t CAF_NET_EXPORT read(stream_socket x, span<byte> buf);

/// Sends data to `x`.
/// @param x A connected endpoint.
/// @param buf Points to the message to send.
/// @returns The number of received bytes on success, 0 if the socket is closed,
///          or -1 in case of an error.
/// @relates stream_socket
/// @post either the result is a `sec` or a positive (non-zero) integer
ptrdiff_t CAF_NET_EXPORT write(stream_socket x, span<const byte> buf);

/// Transmits data from `x` to its peer.
/// @param x A connected endpoint.
/// @param bufs Points to the message to send, scattered across up to 10
///             buffers.
/// @returns The number of written bytes on success, otherwise an error code.
/// @relates stream_socket
/// @post either the result is a `sec` or a positive (non-zero) integer
/// @pre `bufs.size() < 10`
ptrdiff_t CAF_NET_EXPORT write(stream_socket x,
                               std::initializer_list<span<const byte>> bufs);

} // namespace caf::net
