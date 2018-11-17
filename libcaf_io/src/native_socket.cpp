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

#include "caf/io/network/native_socket.hpp"

#include "caf/sec.hpp"
#include "caf/logger.hpp"

#include "caf/detail/call_cfun.hpp"

#include "caf/io/network/protocol.hpp"

#ifdef CAF_WINDOWS
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
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
# include <cerrno>
# include <fcntl.h>
# include <unistd.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <netinet/tcp.h>
#endif

using std::string;

namespace {

auto port_of(sockaddr_in& what) -> decltype(what.sin_port)& {
  return what.sin_port;
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

} // namespace <anonymous>

namespace caf {
namespace io {
namespace network {

#ifdef CAF_WINDOWS
  const int ec_out_of_memory = WSAENOBUFS;
  const int ec_interrupted_syscall = WSAEINTR;
#else
  const int ec_out_of_memory = ENOMEM;
  const int ec_interrupted_syscall = EINTR;
#endif

// platform-dependent SIGPIPE setup
#if defined(CAF_MACOS) || defined(CAF_IOS) || defined(CAF_BSD)
  // Use the socket option but no flags to recv/send on macOS/iOS/BSD.
  const int no_sigpipe_socket_flag = SO_NOSIGPIPE;
  const int no_sigpipe_io_flag = 0;
#elif defined(CAF_WINDOWS)
  // Do nothing on Windows (SIGPIPE does not exist).
  const int no_sigpipe_socket_flag = 0;
  const int no_sigpipe_io_flag = 0;
#else
  // Use flags to recv/send on Linux/Android but no socket option.
  const int no_sigpipe_socket_flag = 0;
  const int no_sigpipe_io_flag = MSG_NOSIGNAL;
#endif

#ifndef CAF_WINDOWS

  int last_socket_error() {
    return errno;
  }

  void close_socket(native_socket fd) {
    close(fd);
  }

  bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == EAGAIN || errcode == EWOULDBLOCK;
  }

  string last_socket_error_as_string() {
    return strerror(errno);
  }

  expected<void> child_process_inherit(native_socket fd, bool new_value) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
    // read flags for fd
    CALL_CFUN(rf, detail::cc_not_minus1, "fcntl", fcntl(fd, F_GETFD));
    // calculate and set new flags
    auto wf = (! new_value) ? (rf | FD_CLOEXEC) : (rf & (~(FD_CLOEXEC)));
    CALL_CFUN(set_res, detail::cc_not_minus1, "fcntl", fcntl(fd, F_SETFD, wf));
    return unit;
  }

  expected<void> nonblocking(native_socket fd, bool new_value) {
    CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
    // read flags for fd
    CALL_CFUN(rf, detail::cc_not_minus1, "fcntl", fcntl(fd, F_GETFL, 0));
    // calculate and set new flags
    auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
    CALL_CFUN(set_res, detail::cc_not_minus1, "fcntl", fcntl(fd, F_SETFL, wf));
    return unit;
  }

  expected<void> allow_sigpipe(native_socket fd, bool new_value) {
    if (no_sigpipe_socket_flag != 0) {
      int value = new_value ? 0 : 1;
      CALL_CFUN(res, detail::cc_zero, "setsockopt",
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
    // note pipe2 is better to avoid races in setting CLOEXEC (but not posix)
    child_process_inherit(pipefds[0], false);
    child_process_inherit(pipefds[1], false);
    return {pipefds[0], pipefds[1]};
  }

#else // CAF_WINDOWS

  int last_socket_error() {
    return WSAGetLastError();
  }

  void close_socket(native_socket fd) {
    closesocket(fd);
  }

  bool would_block_or_temporarily_unavailable(int errcode) {
    return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
  }

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
    string result;
    if (errorText != nullptr) {
      result = errorText;
      // release memory allocated by FormatMessage()
      LocalFree(errorText);
    }
    return result;
  }

  expected<void> child_process_inherit(native_socket fd, bool new_value) {
    // nop; FIXME: possible to implement via SetHandleInformation ?
    return unit;
  }

  expected<void> nonblocking(native_socket fd, bool new_value) {
    u_long mode = new_value ? 1 : 0;
    CALL_CFUN(res, detail::cc_zero, "ioctlsocket",
              ioctlsocket(fd, FIONBIO, &mode));
    return unit;
  }

  expected<void> allow_sigpipe(native_socket, bool) {
    // nop; SIGPIPE does not exist on Windows
    return unit;
  }

  expected<void> allow_udp_connreset(native_socket fd, bool new_value) {
    DWORD bytes_returned = 0;
    CALL_CFUN(res, detail::cc_zero, "WSAIoctl",
              WSAIoctl(fd, _WSAIOW(IOC_VENDOR, 12), &new_value, sizeof(new_value),
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
    socket_size_type addrlen = sizeof(sockaddr_in);
    native_socket socks[2] = {invalid_native_socket, invalid_native_socket};
    CALL_CRITICAL_CFUN(listener, detail::cc_valid_socket, "socket",
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
      close_socket(listener);
      close_socket(socks[0]);
      close_socket(socks[1]);
      WSASetLastError(e);
    });
    // bind listener to a local port
    int reuse = 1;
    CALL_CRITICAL_CFUN(tmp1, detail::cc_zero, "setsockopt",
                       setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                                  reinterpret_cast<char*>(&reuse),
                                  static_cast<int>(sizeof(reuse))));
    CALL_CRITICAL_CFUN(tmp2, detail::cc_zero, "bind",
                       bind(listener, &a.addr,
                            static_cast<int>(sizeof(a.inaddr))));
    // read the port in use: win32 getsockname may only set the port number
    // (http://msdn.microsoft.com/library/ms738543.aspx):
    memset(&a, 0, sizeof(a));
    CALL_CRITICAL_CFUN(tmp3, detail::cc_zero, "getsockname",
                       getsockname(listener, &a.addr, &addrlen));
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_family = AF_INET;
    // set listener to listen mode
    CALL_CRITICAL_CFUN(tmp5, detail::cc_zero, "listen", listen(listener, 1));
    // create read-only end of the pipe
    DWORD flags = 0;
    CALL_CRITICAL_CFUN(read_fd, detail::cc_valid_socket, "WSASocketW",
                       WSASocketW(AF_INET, SOCK_STREAM, 0, nullptr, 0, flags));
    CALL_CRITICAL_CFUN(tmp6, detail::cc_zero, "connect",
                       connect(read_fd, &a.addr,
                               static_cast<int>(sizeof(a.inaddr))));
    // get write-only end of the pipe
    CALL_CRITICAL_CFUN(write_fd, detail::cc_valid_socket, "accept",
                       accept(listener, nullptr, nullptr));
    close_socket(listener);
    guard.disable();
    return std::make_pair(read_fd, write_fd);
  }

#endif

expected<int> send_buffer_size(native_socket fd) {
  int size;
  socket_size_type ret_size = sizeof(size);
  CALL_CFUN(res, detail::cc_zero, "getsockopt",
            getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
            reinterpret_cast<getsockopt_ptr>(&size), &ret_size));
  return size;
}

expected<void> send_buffer_size(native_socket fd, int new_value) {
  CALL_CFUN(res, detail::cc_zero, "setsockopt",
            setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                       reinterpret_cast<setsockopt_ptr>(&new_value),
                       static_cast<socket_size_type>(sizeof(int))));
  return unit;
}

expected<void> tcp_nodelay(native_socket fd, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(fd) << CAF_ARG(new_value));
  int flag = new_value ? 1 : 0;
  CALL_CFUN(res, detail::cc_zero, "setsockopt",
            setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<setsockopt_ptr>(&flag),
                       static_cast<socket_size_type>(sizeof(flag))));
  return unit;
}

bool is_error(signed_size_type res, bool is_nonblock) {
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

expected<string> local_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socket_size_type st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp1, detail::cc_zero, "getsockname", getsockname(fd, sa, &st_len));
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
  socket_size_type st_len = sizeof(st);
  CALL_CFUN(tmp, detail::cc_zero, "getsockname",
            getsockname(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

expected<string> remote_addr_of_fd(native_socket fd) {
  sockaddr_storage st;
  socket_size_type st_len = sizeof(st);
  sockaddr* sa = reinterpret_cast<sockaddr*>(&st);
  CALL_CFUN(tmp, detail::cc_zero, "getpeername", getpeername(fd, sa, &st_len));
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
  socket_size_type st_len = sizeof(st);
  CALL_CFUN(tmp, detail::cc_zero, "getpeername",
            getpeername(fd, reinterpret_cast<sockaddr*>(&st), &st_len));
  return ntohs(port_of(reinterpret_cast<sockaddr&>(st)));
}

// -- shutdown function family -------------------------------------------------

namespace {

#ifdef CAF_WINDOWS
static constexpr int read_channel = SD_RECEIVE;
static constexpr int write_channel = SD_SEND;
static constexpr int both_channels = SD_BOTH;
#else // CAF_WINDOWS
static constexpr int read_channel = SHUT_RD;
static constexpr int write_channel = SHUT_WR;
static constexpr int both_channels = SHUT_RDWR;
#endif // CAF_WINDOWS

} // namespace <anonymous>

void shutdown_read(native_socket fd) {
  ::shutdown(fd, read_channel);
}

void shutdown_write(native_socket fd) {
  ::shutdown(fd, write_channel);
}

void shutdown_both(native_socket fd) {
  ::shutdown(fd, both_channels);
}

} // namespace network
} // namespace io
} // namespace caf
