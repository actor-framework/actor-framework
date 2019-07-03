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

#include "caf/net/pipe_socket.hpp"

#include <cstdio>
#include <cstdlib>

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/net/network_socket.hpp"
#include "caf/sec.hpp"
#include "caf/variant.hpp"

namespace caf {
namespace net {

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
expected<std::pair<pipe_socket, pipe_socket>> make_pipe() {
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

variant<size_t, std::errc> write(pipe_socket x, const void* buf,
                                 size_t buf_size) {
  // On Windows, a pipe consists of two stream sockets.
  return write(network_socket{x.id}, buf, buf_size);
}

variant<size_t, std::errc> read(pipe_socket x, void* buf, size_t buf_size) {
  // On Windows, a pipe consists of two stream sockets.
  return read(network_socket{x.id}, buf, buf_size);
}

#else // CAF_WINDOWS

expected<std::pair<pipe_socket, pipe_socket>> make_pipe() {
  socket_id pipefds[2];
  if (pipe(pipefds) != 0)
    return make_error(sec::network_syscall_failed, "pipe",
                      last_socket_error_as_string());
  auto guard = detail::make_scope_guard([&] {
    close(socket{pipefds[0]});
    close(socket{pipefds[1]});
  });
  // Note: for pipe2 it is better to avoid races by setting CLOEXEC (but not on
  // POSIX).
  if (auto err = child_process_inherit(socket{pipefds[0]}, false))
    return err;
  if (auto err = child_process_inherit(socket{pipefds[1]}, false))
    return err;
  guard.disable();
  return std::make_pair(pipe_socket{pipefds[0]}, pipe_socket{pipefds[1]});
}

variant<size_t, std::errc> write(pipe_socket x, const void* buf,
                                 size_t buf_size) {
  auto res = ::write(x.id, buf, buf_size);
  if (res < 0)
    return static_cast<std::errc>(errno);
  return static_cast<size_t>(res);
}

variant<size_t, std::errc> read(pipe_socket x, void* buf, size_t buf_size) {
  auto res = ::read(x.id, buf, buf_size);
  if (res < 0)
    return static_cast<std::errc>(errno);
  return static_cast<size_t>(res);
}

#endif // CAF_WINDOWS

} // namespace net
} // namespace caf
