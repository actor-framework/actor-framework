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

#include "caf/sec.hpp"
#include "caf/logger.hpp"

#include "caf/io/network/protocol.hpp"
#include "caf/io/network/socket_utils.hpp"

#ifdef CAF_WINDOWS
# include <winsock2.h>
# include <ws2tcpip.h> // socklen_t, etc. (MSVC20xx)
# include <windows.h>
# include <io.h>
#else
# include <cerrno>
# include <netdb.h>
# include <fcntl.h>
# include <sys/types.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <utility>
#endif

using std::string;

namespace {

// predicate for `ccall` meaning "expected result of f is 0"
bool cc_zero(int value) {
  return value == 0;
}

// predicate for `ccall` meaning "expected result of f is not -1"
bool cc_not_minus1(int value) {
  return value != -1;
}
  
  // calls a C functions and returns an error if `predicate(var)`  returns false
#define CALL_CFUN(var, predicate, fun_name, expr)                              \
  auto var = expr;                                                             \
  if (!predicate(var))                                                         \
    return make_error(sec::network_syscall_failed,                             \
                      fun_name, last_socket_error_as_string())
  
#ifdef CAF_WINDOWS
// predicate for `ccall` meaning "expected result of f is a valid socket"
bool cc_valid_socket(caf::io::network::native_socket fd) {
  return fd != caf::io::network::invalid_native_socket;
}

  // calls a C functions and calls exit() if `predicate(var)`  returns false
#define CALL_CRITICAL_CFUN(var, predicate, funname, expr)                      \
  auto var = expr;                                                             \
  if (!predicate(var)) {                                                       \
    fprintf(stderr, "[FATAL] %s:%u: syscall failed: %s returned %s\n",         \
           __FILE__, __LINE__, funname, last_socket_error_as_string().c_str());\
    abort();                                                                   \
  } static_cast<void>(0)
  
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif
#endif // CAF_WINDOWS
  
} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {
  
#ifndef CAF_WINDOWS
  
  string last_socket_error_as_string() {
    return strerror(errno);
  }
  
  expected<void> nonblocking(native_socket fd, bool new_value) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
    // read flags for fd
    CALL_CFUN(rf, cc_not_minus1, "fcntl", fcntl(fd, F_GETFL, 0));
    // calculate and set new flags
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    CALL_CFUN(set_res, cc_not_minus1, "fcntl", fcntl(fd, F_SETFL, wf));
    return unit;
  }
  
  expected<void> allow_sigpipe(native_socket fd, bool new_value) {
    if (no_sigpipe_socket_flag != 0) {
      int value = new_value ? 0 : 1;
      CALL_CFUN(res, cc_zero, "setsockopt",
                setsockopt(fd, SOL_SOCKET, no_sigpipe_socket_flag, &value,
                           static_cast<unsigned>(sizeof(value))));
    }
    return unit;
  }
  
  expected<void> allow_udp_connreset(native_socket, bool) {
    // nop; SIO_UDP_CONNRESET only exists on Windows
    return unit;
  }
  
  std::pair<native_socket, native_socket> create_pipe() {
    int pipefds[2];
    if (pipe(pipefds) != 0) {
      perror("pipe");
      exit(EXIT_FAILURE);
    }
    return {pipefds[0], pipefds[1]};
  }
  
#else // CAF_WINDOWS
  
  string last_socket_error_as_string() {
    LPTSTR errorText = NULL;
    auto hresult = last_socket_error();
    FormatMessage( // use system message tables to retrieve error text
                  FORMAT_MESSAGE_FROM_SYSTEM
                  // allocate buffer on local heap for error text
                  | FORMAT_MESSAGE_ALLOCATE_BUFFER
                  // Important! will fail otherwise, since we're not
                  // (and CANNOT) pass insertion parameters
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, // unused with FORMAT_MESSAGE_FROM_SYSTEM
                  hresult, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  (LPTSTR) & errorText, // output
                  0,                    // minimum size for output buffer
                  nullptr);             // arguments - see note
    std::string result;
    if (errorText != nullptr) {
      result = errorText;
      // release memory allocated by FormatMessage()
      LocalFree(errorText);
    }
    return result;
  }
  
  expected<void> nonblocking(native_socket fd, bool new_value) {
    u_long mode = new_value ? 1 : 0;
    CALL_CFUN(res, cc_zero, "ioctlsocket", ioctlsocket(fd, FIONBIO, &mode));
    return unit;
  }
  
  expected<void> allow_sigpipe(native_socket, bool) {
    // nop; SIGPIPE does not exist on Windows
    return unit;
  }
  
  expected<void> allow_udp_connreset(native_socket fd, bool new_value) {
    DWORD bytes_returned = 0;
    CALL_CFUN(res, cc_zero, "WSAIoctl",
              WSAIoctl(fd, SIO_UDP_CONNRESET, &new_value, sizeof(new_value),
                       NULL, 0, &bytes_returned, NULL, NULL));
    return unit;
  }
  
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
  std::pair<native_socket, native_socket> create_pipe() {
    socklen_t addrlen = sizeof(sockaddr_in);
    native_socket socks[2] = {invalid_native_socket, invalid_native_socket};
    CALL_CRITICAL_CFUN(listener, cc_valid_socket, "socket",
                       socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
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
      closesocket(listener);
      closesocket(socks[0]);
      closesocket(socks[1]);
      WSASetLastError(e);
    });
    // bind listener to a local port
    int reuse = 1;
    CALL_CRITICAL_CFUN(tmp1, cc_zero, "setsockopt",
                       setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                                  reinterpret_cast<char*>(&reuse),
                                  static_cast<int>(sizeof(reuse))));
    CALL_CRITICAL_CFUN(tmp2, cc_zero, "bind",
                       bind(listener, &a.addr,
                            static_cast<int>(sizeof(a.inaddr))));
    // read the port in use: win32 getsockname may only set the port number
    // (http://msdn.microsoft.com/library/ms738543.aspx):
    memset(&a, 0, sizeof(a));
    CALL_CRITICAL_CFUN(tmp3, cc_zero, "getsockname",
                       getsockname(listener, &a.addr, &addrlen));
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;
    // set listener to listen mode
    CALL_CRITICAL_CFUN(tmp5, cc_zero, "listen", listen(listener, 1));
    // create read-only end of the pipe
    DWORD flags = 0;
    CALL_CRITICAL_CFUN(read_fd, cc_valid_socket, "WSASocketW",
                       WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, flags));
    CALL_CRITICAL_CFUN(tmp6, cc_zero, "connect",
                       connect(read_fd, &a.addr,
                               static_cast<int>(sizeof(a.inaddr))));
    // get write-only end of the pipe
    CALL_CRITICAL_CFUN(write_fd, cc_valid_socket, "accept",
                       accept(listener, nullptr, nullptr));
    closesocket(listener);
    guard.disable();
    return std::make_pair(read_fd, write_fd);
  }
  
#endif
  
expected<int> send_buffer_size(native_socket fd) {
  int size;
  socklen_t ret_size = sizeof(size);
  CALL_CFUN(res, cc_zero, "getsockopt",
            getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
            reinterpret_cast<getsockopt_ptr>(&size), &ret_size));
  return size;
}

expected<void> send_buffer_size(native_socket fd, int new_value) {
  CALL_CFUN(res, cc_zero, "setsockopt",
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<setsockopt_ptr>(&new_value),
                       static_cast<socklen_t>(sizeof(int))));
  return unit;
}

expected<void> tcp_nodelay(native_socket fd, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
  int flag = new_value ? 1 : 0;
  CALL_CFUN(res, cc_zero, "setsockopt",
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<setsockopt_ptr>(&flag),
                       static_cast<socklen_t>(sizeof(flag))));
  return unit;
}

bool is_error(ssize_t res, bool is_nonblock) {
  if (res < 0) {
    auto err = last_socket_error();
    if (!is_nonblock || !would_block_or_temporarily_unavailable(err)) {
      return true;
    }
    // don't report an error in case of
    // spurious wakeup or something similar
  }
  return false;
}
  
expected<std::string> local_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp1, cc_zero, "getsockname", getsockname(fd, sa, &st_len));
  char addr[INET6_ADDRSTRLEN] {0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr,
                       addr, sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family,
                    "local_addr_of_fd", sa->sa_family);
}

expected<uint16_t> local_port_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  CALL_CFUN(tmp, cc_zero, "getsockname",
            getsockname(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

expected<std::string> remote_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp, cc_zero, "getpeername", getpeername(fd, sa, &st_len));
  char addr[INET6_ADDRSTRLEN] {0};
  switch (sa->sa_family) {
    case AF_INET:
      return inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(sa)->sin_addr,
                       addr, sizeof(addr));
    case AF_INET6:
      return inet_ntop(AF_INET6,
                       &reinterpret_cast<sockaddr_in6*>(sa)->sin6_addr,
                       addr, sizeof(addr));
    default:
      break;
  }
  return make_error(sec::invalid_protocol_family,
                    "remote_addr_of_fd", sa->sa_family);
}

expected<uint16_t> remote_port_of_fd(native_socket fd) {
  sockaddr_storage st;
  socklen_t st_len = sizeof(st);
  CALL_CFUN(tmp, cc_zero, "getpeername",
            getpeername(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

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

auto port_of(sockaddr& what) -> decltype(port_of(std::declval<sockaddr_in&>())) {
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
  
} // namespace network
} // namespace io
} // namespace caf

