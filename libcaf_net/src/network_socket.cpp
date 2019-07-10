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

#include "caf/net/network_socket.hpp"

#include <cstdint>

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/io/network/protocol.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"
#include "caf/variant.hpp"

namespace {

uint16_t port_of(sockaddr_in& what) {
  return what.sin_port;
}

uint16_t port_of(sockaddr_in6& what) {
  return what.sin6_port;
}

uint16_t port_of(sockaddr& what) {
  switch (what.sa_family) {
    case AF_INET:
      return port_of(reinterpret_cast<sockaddr_in&>(what));
    case AF_INET6:
      return port_of(reinterpret_cast<sockaddr_in6&>(what));
    default:
      break;
  }
  CAF_CRITICAL("invalid protocol family");
}

} // namespace

namespace caf {
namespace net {

#if defined(CAF_MACOS) || defined(CAF_IOS) || defined(CAF_BSD)
#  define CAF_HAS_NOSIGPIPE_SOCKET_FLAG
const int no_sigpipe_io_flag = 0;
#elif defined(CAF_WINDOWS)
const int no_sigpipe_io_flag = 0;
#else
// Use flags to recv/send on Linux/Android but no socket option.
const int no_sigpipe_io_flag = MSG_NOSIGNAL;
#endif

#ifdef CAF_WINDOWS

using setsockopt_ptr = const char*;
using getsockopt_ptr = char*;
using socket_send_ptr = const char*;
using socket_recv_ptr = char*;
using socket_size_type = int;

error keepalive(network_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  char value = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_KEEPALIVE, &value,
                             static_cast<int>(sizeof(value))));
  return none;
}

error allow_sigpipe(network_socket x, bool) {
  if (x == invalid_socket)
    return make_error(sec::network_syscall_failed, "setsockopt",
                      "invalid socket");
  return none;
}

error allow_udp_connreset(network_socket x, bool new_value) {
  DWORD bytes_returned = 0;
  CAF_NET_SYSCALL("WSAIoctl", res, !=, 0,
                  WSAIoctl(x.id, _WSAIOW(IOC_VENDOR, 12), &new_value,
                           sizeof(new_value), NULL, 0, &bytes_returned, NULL,
                           NULL));
  return none;
}
#else // CAF_WINDOWS

using setsockopt_ptr = const void*;
using getsockopt_ptr = void*;
using socket_send_ptr = const void*;
using socket_recv_ptr = void*;
using socket_size_type = unsigned;

error keepalive(network_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  int value = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_KEEPALIVE, &value,
                             static_cast<unsigned>(sizeof(value))));
  return none;
}

error allow_sigpipe(network_socket x, bool new_value) {
#  ifdef CAF_HAS_NOSIGPIPE_SOCKET_FLAG
  int value = new_value ? 0 : 1;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_NOSIGPIPE, &value,
                             static_cast<unsigned>(sizeof(value))));
#  else  // CAF_HAS_NO_SIGPIPE_SOCKET_FLAG
  if (x == invalid_socket)
    return make_error(sec::network_syscall_failed, "setsockopt",
                      "invalid socket");
#  endif // CAF_HAS_NO_SIGPIPE_SOCKET_FLAG
  return none;
}

error allow_udp_connreset(network_socket x, bool) {
  // SIO_UDP_CONNRESET only exists on Windows
  if (x == invalid_socket)
    return make_error(sec::network_syscall_failed, "WSAIoctl",
                      "invalid socket");
  return none;
}

#endif // CAF_WINDOWS

expected<size_t> send_buffer_size(network_socket x) {
  int size = 0;
  socket_size_type ret_size = sizeof(size);
  CAF_NET_SYSCALL("getsockopt", res, !=, 0,
                  getsockopt(x.id, SOL_SOCKET, SO_SNDBUF,
                             reinterpret_cast<getsockopt_ptr>(&size),
                             &ret_size));
  return static_cast<size_t>(size);
}

error send_buffer_size(network_socket x, size_t capacity) {
  auto new_value = static_cast<int>(capacity);
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_SNDBUF,
                             reinterpret_cast<setsockopt_ptr>(&new_value),
                             static_cast<socket_size_type>(sizeof(int))));
  return none;
}

error tcp_nodelay(network_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  int flag = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, IPPROTO_TCP, TCP_NODELAY,
                             reinterpret_cast<setsockopt_ptr>(&flag),
                             static_cast<socket_size_type>(sizeof(flag))));
  return none;
}

expected<std::string> local_addr(network_socket x) {
  sockaddr_storage st;
  socket_size_type st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CAF_NET_SYSCALL("getsockname", tmp1, !=, 0, getsockname(x.id, sa, &st_len));
  char addr[INET6_ADDRSTRLEN]{0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr, addr,
                       sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family, "local_addr", sa->sa_family);
}

expected<uint16_t> local_port(network_socket x) {
  sockaddr_storage st;
  auto st_len = static_cast<socket_size_type>(sizeof(st));
  CAF_NET_SYSCALL("getsockname", tmp, !=, 0,
                  getsockname(x.id, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

expected<std::string> remote_addr(network_socket x) {
  sockaddr_storage st;
  socket_size_type st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CAF_NET_SYSCALL("getpeername", tmp, !=, 0, getpeername(x.id, sa, &st_len));
  char addr[INET6_ADDRSTRLEN]{0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr, addr,
                       sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family, "remote_addr", sa->sa_family);
}

expected<uint16_t> remote_port(network_socket x) {
  sockaddr_storage st;
  socket_size_type st_len = sizeof(st);
  CAF_NET_SYSCALL("getpeername", tmp, !=, 0,
                  getpeername(x.id, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

void shutdown_read(network_socket x) {
  ::shutdown(x.id, 0);
}

void shutdown_write(network_socket x) {
  ::shutdown(x.id, 1);
}

void shutdown(network_socket x) {
  ::shutdown(x.id, 2);
}

variant<size_t, std::errc> write(network_socket x, const void* buf,
                                 size_t buf_size) {
  auto res = ::send(x.id, reinterpret_cast<socket_send_ptr>(buf), buf_size,
                    no_sigpipe_io_flag);
  if (res < 0)
    return static_cast<std::errc>(errno);
  return static_cast<size_t>(res);
}

variant<size_t, std::errc> read(network_socket x, void* buf, size_t buf_size) {
  auto res = ::recv(x.id, reinterpret_cast<socket_recv_ptr>(buf), buf_size,
                    no_sigpipe_io_flag);
  if (res < 0)
    return static_cast<std::errc>(errno);
  return static_cast<size_t>(res);
}

#ifdef CAF_WINDOWS

/**************************************************************************\
 * Based on work of others;                                               *
 * original header:                                                       *
 *                                                                        *
 * Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>              *
 * Redistribution and use in source and binary forms, with or without     *
 * modification, are permitted provided that the following conditions     *
 * are met:                                                               *
 *                                                                        *
 * Redistributions of source code must retain the above copyright notice, *
 * this list of conditions and the following disclaimer.                  *
 *                                                                        *
 * Redistributions in binary form must reproduce the above copyright      *
 * notice, this list of conditions and the following disclaimer in the    *
 * documentation and/or other materials provided with the distribution.   *
 *                                                                        *
 * The name of the author must not be used to endorse or promote products *
 * derived from this software without specific prior written permission.  *
 *                                                                        *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS    *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT      *
 * LIMITED  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR *
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT   *
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT       *
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,  *
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY  *
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT    *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  *
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.   *
 \**************************************************************************/
expected<std::pair<network_socket, network_socket>> make_network_socket_pair() {
  auto addrlen = static_cast<int>(sizeof(sockaddr_in));
  socket_id socks[2] = {invalid_socket_id, invalid_socket_id};
  CAF_NET_SYSCALL("socket", listener, ==, invalid_socket_id,
                  ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  union {
    sockaddr_in inaddr;
    sockaddr addr;
  } a;
  memset(&a, 0, sizeof(a));
  a.inaddr.sin_family = AF_INET;
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_port = 0;
  // makes sure all sockets are closed in case of an error
  auto guard = detail::make_scope_guard([&] {
    auto e = WSAGetLastError();
    close(socket{listener});
    close(socket{socks[0]});
    close(socket{socks[1]});
    WSASetLastError(e);
  });
  // bind listener to a local port
  int reuse = 1;
  CAF_NET_SYSCALL("setsockopt", tmp1, !=, 0,
                  setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                             reinterpret_cast<char*>(&reuse),
                             static_cast<int>(sizeof(reuse))));
  CAF_NET_SYSCALL("bind", tmp2, !=, 0,
                  bind(listener, &a.addr, static_cast<int>(sizeof(a.inaddr))));
  // Read the port in use: win32 getsockname may only set the port number
  // (http://msdn.microsoft.com/library/ms738543.aspx).
  memset(&a, 0, sizeof(a));
  CAF_NET_SYSCALL("getsockname", tmp3, !=, 0,
                  getsockname(listener, &a.addr, &addrlen));
  a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  a.inaddr.sin_family = AF_INET;
  // set listener to listen mode
  CAF_NET_SYSCALL("listen", tmp5, !=, 0, listen(listener, 1));
  // create read-only end of the pipe
  DWORD flags = 0;
  CAF_NET_SYSCALL("WSASocketW", read_fd, ==, invalid_socket_id,
                  WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, flags));
  CAF_NET_SYSCALL("connect", tmp6, !=, 0,
                  connect(read_fd, &a.addr,
                          static_cast<int>(sizeof(a.inaddr))));
  // get write-only end of the pipe
  CAF_NET_SYSCALL("accept", write_fd, ==, invalid_socket_id,
                  accept(listener, nullptr, nullptr));
  close(socket{listener});
  guard.disable();
  return std::make_pair(read_fd, write_fd);
}

#else // CAF_WINDOWS

expected<std::pair<network_socket, network_socket>> make_network_socket_pair() {
  int sockets[2];
  CAF_NET_SYSCALL("socketpair", spair_res, !=, 0,
                  socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
  return std::make_pair(network_socket{sockets[0]}, network_socket{sockets[1]});
}

#endif // CAF_WINDOWS

} // namespace net
} // namespace caf
