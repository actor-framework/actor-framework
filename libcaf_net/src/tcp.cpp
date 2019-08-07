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

#include "caf/net/tcp.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/socket_sys_aliases.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/logger.hpp"
#include "caf/net/interfaces.hpp"
#include "caf/net/socket_guard.hpp"

namespace caf {
namespace net {

namespace {

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
  using sockaddr_type = typename std::conditional<
    Family == AF_INET, sockaddr_in, sockaddr_in6>::type;
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

template <int Family>
bool ip_connect(socket fd, const std::string& host, uint16_t port) {
  CAF_LOG_TRACE("Family =" << (Family == AF_INET ? "AF_INET" : "AF_INET6")
                           << CAF_ARG(fd.id) << CAF_ARG(host));
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  using sockaddr_type = typename std::conditional<
    Family == AF_INET, sockaddr_in, sockaddr_in6>::type;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  inet_pton(Family, host.c_str(), &addr_of(sa));
  family_of(sa) = Family;
  port_of(sa) = htons(port);
  using sa_ptr = const sockaddr*;
  return ::connect(fd.id, reinterpret_cast<sa_ptr>(&sa), sizeof(sa)) == 0;
}

} // namespace

expected<stream_socket> tcp::make_accept_socket(uint16_t port, const char* addr,
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
    auto p = elem.second == ip::v4
               ? new_ip_acceptor_impl<AF_INET>(port, hostname, reuse_addr, any)
               : new_ip_acceptor_impl<AF_INET6>(port, hostname, reuse_addr,
                                                any);
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
    return make_error(sec::cannot_open_port, "tcp socket creation failed", port,
                      addr_str);
  }
  socket_guard sguard{fd};
  CAF_NET_SYSCALL("listen", tmp, !=, 0, listen(fd.id, SOMAXCONN));
  // ok, no errors so far
  CAF_LOG_DEBUG(CAF_ARG(fd.id));
  return socket_cast<stream_socket>(sguard.release());
}

expected<stream_socket> tcp::make_connected_socket(std::string host,
                                                   uint16_t port,
                                                   optional<ip> preferred) {
  CAF_LOG_TRACE(CAF_ARG(host) << CAF_ARG(port) << CAF_ARG(preferred));
  CAF_LOG_DEBUG("try to connect to:" << CAF_ARG(host) << CAF_ARG(port));
  auto res = interfaces::native_address(host, std::move(preferred));
  if (!res) {
    CAF_LOG_DEBUG("no such host");
    return make_error(sec::cannot_connect_to_node, "no such host", host, port);
  }
  auto proto = res->second;
  CAF_ASSERT(proto == ip::v4 || proto == ip::v6);
  int socktype = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
  socktype |= SOCK_CLOEXEC;
#endif
  CAF_NET_SYSCALL("socket", fd, ==, -1,
                  ::socket(proto == ip::v4 ? AF_INET : AF_INET6, socktype, 0));
  child_process_inherit(fd, false);
  socket_guard sguard(fd);
  if (proto == ip::v6) {
    if (ip_connect<AF_INET6>(fd, res->first, port)) {
      CAF_LOG_INFO("successfully connected to (IPv6):" << CAF_ARG(host)
                                                       << CAF_ARG(port));
      return socket_cast<stream_socket>(sguard.release());
    }
    sguard.close();
    // IPv4 fallback
    return make_connected_socket(host, port, ip::v4);
  }
  if (!ip_connect<AF_INET>(fd, res->first, port)) {
    CAF_LOG_WARNING("could not connect to:" << CAF_ARG(host) << CAF_ARG(port));
    return make_error(sec::cannot_connect_to_node, "ip_connect failed", host,
                      port);
  }
  CAF_LOG_INFO("successfully connected to (IPv4):" << CAF_ARG(host)
                                                   << CAF_ARG(port));
  return socket_cast<stream_socket>(sguard.release());
}

expected<stream_socket> tcp::accept(stream_socket x) {
  auto sck = ::accept(x.id, nullptr, nullptr);
  if (sck == net::invalid_socket_id) {
    auto err = net::last_socket_error();
    if (err != std::errc::operation_would_block
        && err != std::errc::resource_unavailable_try_again) {
      return caf::make_error(sec::unavailable_or_would_block);
    }
    return caf::make_error(sec::socket_operation_failed);
  }
  return stream_socket{sck};
}

} // namespace net
} // namespace caf
