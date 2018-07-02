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

#include "caf/expected.hpp"

#include "caf/io/network/native_socket.hpp"

#ifdef CAF_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif // WIN32_LEAN_AND_MEAN
# ifndef NOMINMAX
#   define NOMINMAX
# endif
# ifdef CAF_MINGW
#   undef _WIN32_WINNT
#   undef WINVER
#   define _WIN32_WINNT WindowsVista
#   define WINVER WindowsVista
#   include <w32api.h>
# endif
# include <winsock2.h>
# include <windows.h>
# include <ws2tcpip.h>
# include <ws2ipdef.h>
#else
# include <unistd.h>
# include <cerrno>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
#endif

namespace caf {
namespace io {
namespace network {

  // annoying platform-dependent bootstrapping
#ifdef CAF_WINDOWS
  using setsockopt_ptr = const char*;
  using getsockopt_ptr = char*;
  using socket_send_ptr = const char*;
  using socket_recv_ptr = char*;
  using socklen_t = int;
  using ssize_t = std::make_signed<size_t>::type;
  inline int last_socket_error() { return WSAGetLastError(); }
  inline bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
  }
  constexpr int ec_out_of_memory = WSAENOBUFS;
  constexpr int ec_interrupted_syscall = WSAEINTR;
#else
  using setsockopt_ptr = const void*;
  using getsockopt_ptr = void*;
  using socket_send_ptr = const void*;
  using socket_recv_ptr = void*;
  inline void closesocket(int fd) { close(fd); }
  inline int last_socket_error() { return errno; }
  inline bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == EAGAIN || errcode == EWOULDBLOCK;
  }
  constexpr int ec_out_of_memory = ENOMEM;
  constexpr int ec_interrupted_syscall = EINTR;
#endif

  // platform-dependent SIGPIPE setup
#if defined(CAF_MACOS) || defined(CAF_IOS) || defined(CAF_BSD)
  // Use the socket option but no flags to recv/send on macOS/iOS/BSD.
  constexpr int no_sigpipe_socket_flag = SO_NOSIGPIPE;
  constexpr int no_sigpipe_io_flag = 0;
#elif defined(CAF_WINDOWS)
  // Do nothing on Windows (SIGPIPE does not exist).
  constexpr int no_sigpipe_socket_flag = 0;
  constexpr int no_sigpipe_io_flag = 0;
#else
  // Use flags to recv/send on Linux/Android but no socket option.
  constexpr int no_sigpipe_socket_flag = 0;
  constexpr int no_sigpipe_io_flag = MSG_NOSIGNAL;
#endif

/// Returns the last socket error as human-readable string.
std::string last_socket_error_as_string();

/// Creates two connected sockets. The former is the read handle
/// and the latter is the write handle.
std::pair<native_socket, native_socket> create_pipe();

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
bool is_error(ssize_t res, bool is_nonblock);

/// Returns the locally assigned port of `fd`.
expected<uint16_t> local_port_of_fd(native_socket fd);

/// Returns the locally assigned address of `fd`.
expected<std::string> local_addr_of_fd(native_socket fd);

/// Returns the port used by the remote host of `fd`.
expected<uint16_t> remote_port_of_fd(native_socket fd);

/// Returns the remote host address of `fd`.
expected<std::string> remote_addr_of_fd(native_socket fd);

/// @cond PRIVTE

// Utility functions to privde access to sockaddr fields.
  
auto addr_of(sockaddr_in& what) -> decltype(what.sin_addr)&;

auto family_of(sockaddr_in& what) -> decltype(what.sin_family)&;

auto port_of(sockaddr_in& what) -> decltype(what.sin_port)&;

auto addr_of(sockaddr_in6& what) -> decltype(what.sin6_addr)&;

auto family_of(sockaddr_in6& what) -> decltype(what.sin6_family)&;

auto port_of(sockaddr_in6& what) -> decltype(what.sin6_port)&;

auto port_of(sockaddr& what) -> decltype(port_of(std::declval<sockaddr_in&>()));

/// @endcond
  
} // namespace network
} // namespace io
} // namespace caf
