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
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
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
#endif

#ifdef CAF_WINDOWS

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

} // namespace net
} // namespace caf
