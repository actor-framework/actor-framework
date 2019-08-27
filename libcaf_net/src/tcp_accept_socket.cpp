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

#include "caf/net/tcp_accept_socket.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/sockaddr_members.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ip_address.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/logger.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/sec.hpp"

namespace caf {
namespace net {

namespace {

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
  auto sguard = make_socket_guard(socket{fd});
  if (reuse_addr) {
    int on = 1;
    CAF_NET_SYSCALL("setsockopt", tmp1, !=, 0,
                    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<setsockopt_ptr>(&on),
                               static_cast<socket_size_type>(sizeof(on))));
  }
  using sockaddr_type = typename std::conditional<
    Family == AF_INET, sockaddr_in, sockaddr_in6>::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  detail::family_of(sa) = Family;
  if (any)
    set_inaddr_any(fd, sa);
  CAF_NET_SYSCALL("inet_pton", tmp, !=, 1,
                  inet_pton(Family, addr, &detail::addr_of(sa)));
  detail::port_of(sa) = htons(port);
  CAF_NET_SYSCALL("bind", res, !=, 0,
                  bind(fd, reinterpret_cast<sockaddr*>(&sa),
                       static_cast<socket_size_type>(sizeof(sa))));
  return sguard.release();
}

} // namespace

expected<tcp_accept_socket> make_accept_socket(ip_address addr, uint16_t port,
                                               bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << to_string(addr));
  auto h = addr.embeds_v4() ? to_string(addr.embedded_v4()) : to_string(addr);
  bool any = addr.zero();
  auto p = addr.embeds_v4()
             ? new_ip_acceptor_impl<AF_INET>(port, h.c_str(), reuse_addr, any)
             : new_ip_acceptor_impl<AF_INET6>(port, h.c_str(), reuse_addr, any);
  if (!p) {
    CAF_LOG_WARNING("could not open tcp socket on:" << CAF_ARG(port)
                                                    << CAF_ARG(h));
    return make_error(sec::cannot_open_port, "tcp socket creation failed", port,
                      h);
  }
  auto fd = socket_cast<tcp_accept_socket>(*p);
  auto sguard = make_socket_guard(fd);
  CAF_NET_SYSCALL("listen", tmp, !=, 0, listen(fd.id, SOMAXCONN));
  CAF_LOG_DEBUG(CAF_ARG(fd.id));
  return sguard.release();
}

expected<tcp_accept_socket> make_accept_socket(const uri::authority_type& auth,
                                               bool reuse_addr) {
  if (auto ip = get_if<ip_address>(&auth.host))
    return make_accept_socket(*ip, auth.port, reuse_addr);
  auto host = get<std::string>(auth.host);
  auto addrs = ip::resolve(host);
  if (addrs.empty())
    return make_error(sec::cannot_open_port, "No local interface available",
                      to_string(auth));
  for (auto& addr : addrs) {
    if (auto sock = make_accept_socket(addr, auth.port, reuse_addr))
      return *sock;
  }
  return make_error(sec::cannot_open_port, "tcp socket creation failed",
                    to_string(auth));
}

expected<tcp_stream_socket> accept(tcp_accept_socket x) {
  auto sck = ::accept(x.id, nullptr, nullptr);
  if (sck == net::invalid_socket_id) {
    auto err = net::last_socket_error();
    if (err != std::errc::operation_would_block
        && err != std::errc::resource_unavailable_try_again) {
      return caf::make_error(sec::unavailable_or_would_block);
    }
    return caf::make_error(sec::socket_operation_failed);
  }
  return tcp_stream_socket{sck};
}

} // namespace net
} // namespace caf
