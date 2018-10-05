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

#include <string>
#include <cstdint>

#include "caf/config.hpp"
#include "caf/expected.hpp"

namespace caf {
namespace io {
namespace network {

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

using signed_size_type = std::make_signed<size_t>::type;

// More bootstrapping.
extern const int ec_out_of_memory;
extern const int ec_interrupted_syscall;
extern const int no_sigpipe_socket_flag;
extern const int no_sigpipe_io_flag;

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
int last_socket_error();

/// Close socket `fd`.
void close_socket(native_socket fd);

/// Returns true if `errcode` indicates that an operation would
/// block or return nothing at the moment and can be tried again
/// at a later point.
bool would_block_or_temporarily_unavailable(int errcode);

/// Returns the last socket error as human-readable string.
std::string last_socket_error_as_string();

/// Creates two connected sockets. The former is the read handle
/// and the latter is the write handle.
std::pair<native_socket, native_socket> create_pipe();

/// Sets fd to be inherited by child processes if `new_value == true`
/// or not if `new_value == false`.  Not implemented on Windows.
/// throws `network_error` on error
expected<void> child_process_inherit(native_socket fd, bool new_value);

/// Sets fd to nonblocking if `set_nonblocking == true`
/// or to blocking if `set_nonblocking == false`
/// throws `network_error` on error
expected<void> nonblocking(native_socket fd, bool new_value);

/// Enables or disables Nagle's algorithm on `fd`.
/// @throws network_error
expected<void> tcp_nodelay(native_socket fd, bool new_value);

/// Enables or disables `SIGPIPE` events from `fd`.
expected<void> allow_sigpipe(native_socket fd, bool new_value);

/// Enables or disables `SIO_UDP_CONNRESET`error on `fd`.
expected<void> allow_udp_connreset(native_socket fd, bool new_value);

/// Get the socket buffer size for `fd`.
expected<int> send_buffer_size(native_socket fd);

/// Set the socket buffer size for `fd`.
expected<void> send_buffer_size(native_socket fd, int new_value);

/// Convenience functions for checking the result of `recv` or `send`.
bool is_error(signed_size_type res, bool is_nonblock);

/// Returns the locally assigned port of `fd`.
expected<uint16_t> local_port_of_fd(native_socket fd);

/// Returns the locally assigned address of `fd`.
expected<std::string> local_addr_of_fd(native_socket fd);

/// Returns the port used by the remote host of `fd`.
expected<uint16_t> remote_port_of_fd(native_socket fd);

/// Returns the remote host address of `fd`.
expected<std::string> remote_addr_of_fd(native_socket fd);

} // namespace network
} // namespace io
} // namespace caf
