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

#include "caf/net/socket.hpp"

#include "caf/config.hpp"
#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/logger.hpp"

namespace caf {
namespace net {

#ifdef CAF_WINDOWS

void close(socket fd) {
  closesocket(fd.id);
}

int last_socket_error() {
  return WSAGetLastError();
}

std::string socket_error_as_string(int errcode) {
  LPTSTR errorText = NULL;
  FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER
                  | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, errcode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPTSTR) &errorText, 0, nullptr);
  std::string result;
  if (errorText != nullptr) {
    result = errorText;
    // Release memory allocated by FormatMessage().
    LocalFree(errorText);
  }
  return result;
}

std::string last_socket_error_as_string() {
  return socket_error_as_string(last_socket_error());
}

bool would_block_or_temporarily_unavailable(int errcode) {
  return errcode == WSAEWOULDBLOCK || errcode == WSATRY_AGAIN;
}

error child_process_inherit(socket x, bool) {
  // TODO: possible to implement via SetHandleInformation?
  if (x == invalid_socket)
    return make_error(sec::network_syscall_failed, "ioctlsocket",
                      "invalid socket");
  return none;
}

error nonblocking(socket x, bool new_value) {
  u_long mode = new_value ? 1 : 0;
  CAF_NET_SYSCALL("ioctlsocket", res, !=, 0, ioctlsocket(x.id, FIONBIO, &mode));
  return none;
}

#else // CAF_WINDOWS

void close(socket fd) {
  ::close(fd.id);
}

int last_socket_error() {
  return errno;
}

std::string socket_error_as_string(int error_code) {
  return strerror(error_code);
}

std::string last_socket_error_as_string() {
  return strerror(errno);
}

bool would_block_or_temporarily_unavailable(int errcode) {
  return errcode == EAGAIN || errcode == EWOULDBLOCK;
}

error child_process_inherit(socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  // read flags for x
  CAF_NET_SYSCALL("fcntl", rf, ==, -1, fcntl(x.id, F_GETFD));
  // calculate and set new flags
  auto wf = !new_value ? rf | FD_CLOEXEC : rf & ~(FD_CLOEXEC);
  CAF_NET_SYSCALL("fcntl", set_res, ==, -1, fcntl(x.id, F_SETFD, wf));
  return none;
}

error nonblocking(socket x, bool new_value) {
  CAF_LOG_TRACE(CAF_ARG(x) << CAF_ARG(new_value));
  // read flags for x
  CAF_NET_SYSCALL("fcntl", rf, ==, -1, fcntl(x.id, F_GETFL, 0));
  // calculate and set new flags
  auto wf = new_value ? (rf | O_NONBLOCK) : (rf & (~(O_NONBLOCK)));
  CAF_NET_SYSCALL("fcntl", set_res, ==, -1, fcntl(x.id, F_SETFL, wf));
  return none;
}

#endif // CAF_WINDOWS

bool is_error(std::make_signed<size_t>::type res, bool is_nonblock) {
  if (res < 0) {
    auto err = last_socket_error();
    return !is_nonblock || !would_block_or_temporarily_unavailable(err);
  }
  return false;
}

} // namespace net
} // namespace caf
