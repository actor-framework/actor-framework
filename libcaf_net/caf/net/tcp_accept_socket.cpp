// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/tcp_accept_socket.hpp"

#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"
#include "caf/net/tcp_stream_socket.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/sockaddr_members.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ip_address.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/logger.hpp"
#include "caf/sec.hpp"

namespace caf::net {

namespace {

error set_inaddr_any(socket, sockaddr_in& sa) {
  sa.sin_addr.s_addr = INADDR_ANY;
  return none;
}

error set_inaddr_any(socket x, sockaddr_in6& sa) {
  sa.sin6_addr = in6addr_any;
  // Also accept ipv4 connections on this socket.
  int off = 0;
  CAF_NET_SYSCALL("setsockopt", res, !=, 0,
                  setsockopt(x.id, IPPROTO_IPV6, IPV6_V6ONLY,
                             reinterpret_cast<setsockopt_ptr>(&off),
                             static_cast<socket_size_type>(sizeof(off))));
  return none;
}

template <int Family>
expected<tcp_accept_socket> new_tcp_acceptor_impl(uint16_t port,
                                                  const char* addr,
                                                  bool reuse_addr, bool any) {
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  CAF_LOG_TRACE(CAF_ARG(port) << ", addr = " << (addr ? addr : "nullptr"));
  int socktype = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
  socktype |= SOCK_CLOEXEC;
#endif
  CAF_NET_SYSCALL("socket", fd, ==, -1, ::socket(Family, socktype, 0));
  tcp_accept_socket sock{fd};
  // sguard closes the socket in case of exception
  auto sguard = make_socket_guard(tcp_accept_socket{fd});
  if (auto err = child_process_inherit(sock, false))
    return err;
  if (reuse_addr) {
    int on = 1;
    CAF_NET_SYSCALL("setsockopt", tmp1, !=, 0,
                    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                               reinterpret_cast<setsockopt_ptr>(&on),
                               static_cast<socket_size_type>(sizeof(on))));
  }
  using sockaddr_type =
    typename std::conditional<Family == AF_INET, sockaddr_in,
                              sockaddr_in6>::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  detail::family_of(sa) = Family;
  if (any)
    if (auto err = set_inaddr_any(sock, sa))
      return err;
  CAF_NET_SYSCALL("inet_pton", tmp, !=, 1,
                  inet_pton(Family, addr, &detail::addr_of(sa)));
  detail::port_of(sa) = htons(port);
  CAF_NET_SYSCALL("bind", res, !=, 0,
                  bind(fd, reinterpret_cast<sockaddr*>(&sa),
                       static_cast<socket_size_type>(sizeof(sa))));
  CAF_LOG_DEBUG("bound socket" << fd << "to listen on port" << port);
  return sguard.release();
}

} // namespace

expected<tcp_accept_socket> make_tcp_accept_socket(ip_endpoint node,
                                                   bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(node) << CAF_ARG(reuse_addr));
  auto addr = to_string(node.address());
  bool is_v4 = node.address().embeds_v4();
  bool is_zero = is_v4 ? node.address().embedded_v4().bits() == 0
                       : node.address().zero();
  auto make_acceptor = is_v4 ? new_tcp_acceptor_impl<AF_INET>
                             : new_tcp_acceptor_impl<AF_INET6>;
  if (auto p = make_acceptor(node.port(), addr.c_str(), reuse_addr, is_zero)) {
    auto sock = socket_cast<tcp_accept_socket>(*p);
    auto sguard = make_socket_guard(sock);
    CAF_NET_SYSCALL("listen", tmp, !=, 0, listen(sock.id, SOMAXCONN));
    CAF_LOG_DEBUG(CAF_ARG(sock.id));
    return sguard.release();
  } else {
    CAF_LOG_WARNING("could not create tcp socket:" << CAF_ARG(node)
                                                   << CAF_ARG(p.error()));
    return make_error(sec::cannot_open_port, "tcp socket creation failed",
                      to_string(node), std::move(p.error()));
  }
}

expected<tcp_accept_socket>
make_tcp_accept_socket(const uri::authority_type& node, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(node) << CAF_ARG(reuse_addr));
  if (auto ip = std::get_if<ip_address>(&node.host))
    return make_tcp_accept_socket(ip_endpoint{*ip, node.port}, reuse_addr);
  const auto& host = std::get<std::string>(node.host);
  if (host.empty()) {
    // For empty strings, try IPv6::any and use IPv4::any as fallback.
    auto v6_any = ip_address{{0}, {0}};
    auto v4_any = ip_address{make_ipv4_address(0, 0, 0, 0)};
    if (auto sock = make_tcp_accept_socket(ip_endpoint{v6_any, node.port},
                                           reuse_addr))
      return *sock;
    return make_tcp_accept_socket(ip_endpoint{v4_any, node.port}, reuse_addr);
  }
  auto addrs = ip::local_addresses(host);
  if (addrs.empty())
    return make_error(sec::cannot_open_port, "no local interface available",
                      to_string(node));
  // Prefer ipv6 addresses.
  std::stable_sort(std::begin(addrs), std::end(addrs),
                   [](const ip_address& lhs, const ip_address& rhs) {
                     if (lhs.embeds_v4())
                       return rhs.embeds_v4() ? lhs < rhs : false;
                     return rhs.embeds_v4() ? true : lhs < rhs;
                   });
  for (auto& addr : addrs) {
    if (auto sock = make_tcp_accept_socket(ip_endpoint{addr, node.port},
                                           reuse_addr))
      return *sock;
  }
  return make_error(sec::cannot_open_port, "tcp socket creation failed",
                    to_string(node));
}

expected<tcp_accept_socket>
make_tcp_accept_socket(uint16_t port, std::string addr, bool reuse_addr) {
  CAF_LOG_TRACE(CAF_ARG(port) << CAF_ARG(addr) << CAF_ARG(reuse_addr));
  uri::authority_type auth;
  auth.port = port;
  auth.host = std::move(addr);
  return make_tcp_accept_socket(auth, reuse_addr);
}

expected<tcp_stream_socket> accept(tcp_accept_socket x) {
  CAF_LOG_TRACE(CAF_ARG(x));
  auto sock = ::accept(x.id, nullptr, nullptr);
  if (sock == net::invalid_socket_id) {
    auto err = net::last_socket_error();
    if (err != std::errc::operation_would_block
        && err != std::errc::resource_unavailable_try_again) {
      return caf::make_error(sec::unavailable_or_would_block);
    }
    return caf::make_error(sec::socket_operation_failed, "tcp accept failed");
  }
  CAF_LOG_DEBUG("accepted TCP socket" << sock << "on accept socket" << x.id);
  return tcp_stream_socket{sock};
}

} // namespace caf::net
