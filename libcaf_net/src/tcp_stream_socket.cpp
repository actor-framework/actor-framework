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

#include "caf/net/tcp_stream_socket.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/sockaddr_members.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/logger.hpp"
#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/sec.hpp"
#include "caf/variant.hpp"

namespace caf::net {

namespace {

template <int Family>
bool ip_connect(stream_socket fd, std::string host, uint16_t port) {
  CAF_LOG_TRACE("Family =" << (Family == AF_INET ? "AF_INET" : "AF_INET6")
                           << CAF_ARG(fd.id) << CAF_ARG(host) << CAF_ARG(port));
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  using sockaddr_type = typename std::conditional<
    Family == AF_INET, sockaddr_in, sockaddr_in6>::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  inet_pton(Family, host.c_str(), &detail::addr_of(sa));
  detail::family_of(sa) = Family;
  detail::port_of(sa) = htons(port);
  using sa_ptr = const sockaddr*;
  return ::connect(fd.id, reinterpret_cast<sa_ptr>(&sa), sizeof(sa)) == 0;
}

} // namespace

expected<tcp_stream_socket> make_connected_tcp_stream_socket(ip_endpoint node) {
  CAF_LOG_DEBUG("tcp connect to: " << to_string(node));
  auto proto = node.address().embeds_v4() ? AF_INET : AF_INET6;
  int socktype = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
  socktype |= SOCK_CLOEXEC;
#endif
  CAF_NET_SYSCALL("socket", fd, ==, -1, ::socket(proto, socktype, 0));
  tcp_stream_socket sock{fd};
  child_process_inherit(sock, false);
  auto sguard = make_socket_guard(sock);
  if (proto == AF_INET6) {
    if (ip_connect<AF_INET6>(sock, to_string(node.address()), node.port())) {
      CAF_LOG_INFO("successfully connected to (IPv6):" << to_string(node));
      return sguard.release();
    }
  } else if (ip_connect<AF_INET>(sock, to_string(node.address().embedded_v4()),
                                 node.port())) {
    CAF_LOG_INFO("successfully connected to (IPv4):" << to_string(node));
    return sguard.release();
  }
  CAF_LOG_WARNING("could not connect to: " << to_string(node));
  return make_error(sec::cannot_connect_to_node);
}

expected<tcp_stream_socket>
make_connected_tcp_stream_socket(const uri::authority_type& node) {
  auto port = node.port;
  if (port == 0)
    return make_error(sec::cannot_connect_to_node, "port is zero");
  std::vector<ip_address> addrs;
  if (auto str = get_if<std::string>(&node.host))
    addrs = ip::resolve(*str);
  else if (auto addr = get_if<ip_address>(&node.host))
    addrs.push_back(*addr);
  if (addrs.empty())
    return make_error(sec::cannot_connect_to_node, "empty authority");
  for (auto& addr : addrs) {
    if (auto sock = make_connected_tcp_stream_socket(ip_endpoint{addr, port}))
      return *sock;
  }
  return make_error(sec::cannot_connect_to_node, to_string(node));
}

} // namespace caf::net
