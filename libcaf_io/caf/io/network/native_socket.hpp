// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/io_export.hpp"
#include "caf/expected.hpp"

#include <cstdint>
#include <string>

namespace caf::io::network {

// Annoying platform-dependent bootstrapping.
#ifdef CAF_WINDOWS
using setsockopt_ptr = const char*;
using getsockopt_ptr = char*;
using socket_send_ptr = const char*;
using socket_recv_ptr = char*;
using socket_size_type = int;
#else
using setsockopt_ptr = const void*;
using getsockopt_ptr = void*;
using socket_send_ptr = const void*;
using socket_recv_ptr = void*;
using socket_size_type = unsigned;
#endif

using signed_size_type = std::make_signed_t<size_t>;

// More bootstrapping.
CAF_IO_EXPORT extern const int ec_out_of_memory;
CAF_IO_EXPORT extern const int ec_interrupted_syscall;
CAF_IO_EXPORT extern const int no_sigpipe_socket_flag;
CAF_IO_EXPORT extern const int no_sigpipe_io_flag;

#ifdef CAF_WINDOWS
using native_socket = size_t;
constexpr native_socket invalid_native_socket = static_cast<native_socket>(-1);
inline int64_t int64_from_native_socket(native_socket sock) {
  return sock == invalid_native_socket ? -1 : static_cast<int64_t>(sock);
}
#else
using native_socket = int;
constexpr native_socket invalid_native_socket = -1;
inline int64_t int64_from_native_socket(native_socket sock) {
  return static_cast<int64_t>(sock);
}
#endif

/// Returns the last socket error as an integer.
CAF_IO_EXPORT int last_socket_error();

/// Close socket `fd`.
CAF_IO_EXPORT void close_socket(native_socket fd);

/// Returns true if `errcode` indicates that an operation would
/// block or return nothing at the moment and can be tried again
/// at a later point.
CAF_IO_EXPORT bool would_block_or_temporarily_unavailable(int errcode);

/// Returns the last socket error as human-readable string.
CAF_IO_EXPORT std::string last_socket_error_as_string();

/// Returns a human-readable string for a given socket error.
CAF_IO_EXPORT std::string socket_error_as_string(int errcode);

/// Creates two connected sockets. The former is the read handle
/// and the latter is the write handle.
CAF_IO_EXPORT std::pair<native_socket, native_socket> create_pipe();

/// Sets fd to be inherited by child processes if `new_value == true`
/// or not if `new_value == false`.  Not implemented on Windows.
/// throws `network_error` on error
CAF_IO_EXPORT expected<void> child_process_inherit(native_socket fd,
                                                   bool new_value);

/// Enables keepalive on `fd`. Throws `network_error` on error.
CAF_IO_EXPORT expected<void> keepalive(native_socket fd, bool new_value);

/// Sets fd to nonblocking if `set_nonblocking == true`
/// or to blocking if `set_nonblocking == false`
/// throws `network_error` on error
CAF_IO_EXPORT expected<void> nonblocking(native_socket fd, bool new_value);

/// Enables or disables Nagle's algorithm on `fd`.
/// @throws network_error
CAF_IO_EXPORT expected<void> tcp_nodelay(native_socket fd, bool new_value);

/// Enables or disables `SIGPIPE` events from `fd`.
CAF_IO_EXPORT expected<void> allow_sigpipe(native_socket fd, bool new_value);

/// Enables or disables `SIO_UDP_CONNRESET`error on `fd`.
CAF_IO_EXPORT expected<void> allow_udp_connreset(native_socket fd,
                                                 bool new_value);

/// Get the socket buffer size for `fd`.
CAF_IO_EXPORT expected<int> send_buffer_size(native_socket fd);

/// Set the socket buffer size for `fd`.
CAF_IO_EXPORT expected<void> send_buffer_size(native_socket fd, int new_value);

/// Convenience functions for checking the result of `recv` or `send`.
CAF_IO_EXPORT bool is_error(signed_size_type res, bool is_nonblock);

/// Returns the locally assigned port of `fd`.
CAF_IO_EXPORT expected<uint16_t> local_port_of_fd(native_socket fd);

/// Returns the locally assigned address of `fd`.
CAF_IO_EXPORT expected<std::string> local_addr_of_fd(native_socket fd);

/// Returns the port used by the remote host of `fd`.
CAF_IO_EXPORT expected<uint16_t> remote_port_of_fd(native_socket fd);

/// Returns the remote host address of `fd`.
CAF_IO_EXPORT expected<std::string> remote_addr_of_fd(native_socket fd);

/// Closes the read channel for a socket.
CAF_IO_EXPORT void shutdown_read(native_socket fd);

/// Closes the write channel for a socket.
CAF_IO_EXPORT void shutdown_write(native_socket fd);

/// Closes the both read and write channel for a socket.
CAF_IO_EXPORT void shutdown_both(native_socket fd);

} // namespace caf::io::network
