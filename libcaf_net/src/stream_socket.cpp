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

#include "caf/net/stream_socket.hpp"

#include "caf/byte.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/logger.hpp"
#include "caf/net/interfaces.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/socket.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/span.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

namespace {

// Save ourselves some typing.
constexpr auto ipv4 = caf::net::ip::v4;

auto addr_of(sockaddr_in& what) -> decltype(what.sin_addr)& {
  return what.sin_addr;
}

auto family_of(sockaddr_in& what) -> decltype(what.sin_family)& {
  return what.sin_family;
}

auto port_of(sockaddr_in& what) -> decltype(what.sin_port)& {
  return what.sin_port;
}

auto addr_of(sockaddr_in6& what) -> decltype(what.sin6_addr)& {
  return what.sin6_addr;
}

auto family_of(sockaddr_in6& what) -> decltype(what.sin6_family)& {
  return what.sin6_family;
}

auto port_of(sockaddr_in6& what) -> decltype(what.sin6_port)& {
  return what.sin6_port;
}

expected<void> set_inaddr_any(socket, sockaddr_in& sa) {
  sa.sin_addr.s_addr = INADDR_ANY;
  return unit;
}

expected<void> set_inaddr_any(socket x, sockaddr_in6& sa) {
  sa.sin6_addr = in6addr_any;
  // also accept ipv4 requests on this socket
  int off = 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
            setsockopt(x.id, IPPROTO_IPV6, IPV6_V6ONLY,
                       reinterpret_cast<setsockopt_ptr>(&off),
                       static_cast<socket_size_type>(sizeof(off))));
  return unit;
}

template <int Family, int SockType = SOCK_STREAM>
caf::expected<socket> new_ip_acceptor_impl(uint16_t port, const char* addr,
                                           bool reuse_addr, bool any) {
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  int socktype = SockType;
#ifdef SOCK_CLOEXEC
  socktype |= SOCK_CLOEXEC;
#endif
  CAF_NET_SYSCALL("socket", fd, ==, -1, ::socket(Family, socktype, 0));
  child_process_inherit(fd, false);
  // sguard closes the socket in case of exception
  socket_guard sguard{fd};
  if (reuse_addr) {
    int on = 1;
    CAF_NET_SYSCALL("setsockopt", tmp1, !=, 0,
                    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<setsockopt_ptr>(&on),
                               static_cast<socket_size_type>(sizeof(on))));
  }
  using sockaddr_type =
    typename std::conditional<
      Family == AF_INET,
      sockaddr_in,
      sockaddr_in6
    >::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  family_of(sa) = Family;
  if (any)
    set_inaddr_any(fd, sa);
  CAF_NET_SYSCALL("inet_pton", tmp, !=, 1,
                  inet_pton(Family, addr, &addr_of(sa)));
  port_of(sa) = htons(port);
  CAF_NET_SYSCALL("bind", res, !=, 0,
            bind(fd, reinterpret_cast<sockaddr*>(&sa),
                 static_cast<socket_size_type>(sizeof(sa))));
  return sguard.release();
}

} // namespace

#ifdef CAF_WINDOWS

constexpr int no_sigpipe_io_flag = 0;

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
expected<std::pair<stream_socket, stream_socket>> make_stream_socket_pair() {
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

error keepalive(stream_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  char value = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_KEEPALIVE, &value,
                             static_cast<int>(sizeof(value))));
  return none;
}

#else // CAF_WINDOWS

#  if defined(CAF_MACOS) || defined(CAF_IOS) || defined(CAF_BSD)
constexpr int no_sigpipe_io_flag = 0;
#  else
constexpr int no_sigpipe_io_flag = MSG_NOSIGNAL;
#  endif

expected<std::pair<stream_socket, stream_socket>> make_stream_socket_pair() {
  int sockets[2];
  CAF_NET_SYSCALL("socketpair", spair_res, !=, 0,
                  socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
  return std::make_pair(stream_socket{sockets[0]}, stream_socket{sockets[1]});
}

error keepalive(stream_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  int value = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, SOL_SOCKET, SO_KEEPALIVE, &value,
                             static_cast<unsigned>(sizeof(value))));
  return none;
}

#endif // CAF_WINDOWS

error nodelay(stream_socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  int flag = new_value ? 1 : 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, IPPROTO_TCP, TCP_NODELAY,
                             reinterpret_cast<setsockopt_ptr>(&flag),
                             static_cast<socket_size_type>(sizeof(flag))));
  return none;
}

variant<size_t, sec> read(stream_socket x, span<byte> buf) {
  auto res = ::recv(x.id, reinterpret_cast<socket_recv_ptr>(buf.data()),
                    buf.size(), no_sigpipe_io_flag);
  return check_stream_socket_io_res(res);
}

variant<size_t, sec> write(stream_socket x, span<const byte> buf) {
  auto res = ::send(x.id, reinterpret_cast<socket_send_ptr>(buf.data()),
                    buf.size(), no_sigpipe_io_flag);
  return check_stream_socket_io_res(res);
}

variant<size_t, sec>
check_stream_socket_io_res(std::make_signed<size_t>::type res) {
  if (res == 0)
    return sec::socket_disconnected;
  if (res < 0) {
    auto code = last_socket_error();
    if (code == std::errc::operation_would_block
        || code == std::errc::resource_unavailable_try_again)
      return sec::unavailable_or_would_block;
    return sec::socket_operation_failed;
  }
  return static_cast<size_t>(res);
}

expected<stream_socket> make_accept_socket(uint16_t port, const char* addr,
                                           bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  auto addrs = interfaces::server_address(port, addr);
  auto addr_str = std::string{addr == nullptr ? "" : addr};
  if (addrs.empty())
    return make_error(sec::cannot_open_port, "No local interface available",
                      addr_str);
  bool any = addr_str.empty() || addr_str == "::" || addr_str == "0.0.0.0";
  socket fd = invalid_socket;
  for (auto& elem : addrs) {
    auto hostname = elem.first.c_str();
    auto p = elem.second == ipv4
           ? new_ip_acceptor_impl<AF_INET>(port, hostname, reuse_addr, any)
           : new_ip_acceptor_impl<AF_INET6>(port, hostname, reuse_addr, any);
    if (!p) {
      CAF_LOG_DEBUG(p.error());
      continue;
    }
    fd = *p;
    break;
  }
  if (fd == invalid_socket_id) {
    CAF_LOG_WARNING("could not open tcp socket on:" << CAF_ARG(port)
                    << CAF_ARG(addr_str));
    return make_error(sec::cannot_open_port, "tcp socket creation failed",
                      port, addr_str);
  }
  socket_guard sguard{fd.id};
  CAF_NET_SYSCALL("listen", tmp2, !=, 0, listen(fd.id, SOMAXCONN));
  // ok, no errors so far
  CAF_LOG_DEBUG(CAF_ARG(fd.id));
  return socket_cast<stream_socket>(sguard.release());
}

} // namespace net
} // namespace caf
