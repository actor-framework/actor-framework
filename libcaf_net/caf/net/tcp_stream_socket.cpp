// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/tcp_stream_socket.hpp"

#include "caf/net/ip.hpp"
#include "caf/net/socket_guard.hpp"

#include "caf/detail/net_syscall.hpp"
#include "caf/detail/sockaddr_members.hpp"
#include "caf/detail/socket_sys_includes.hpp"
#include "caf/expected.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/log/net.hpp"
#include "caf/sec.hpp"

#include <algorithm>
#include <thread>

#ifndef CAF_WINDOWS
#  include <poll.h>
#endif

#ifdef CAF_WINDOWS
#  define POLL_FN ::WSAPoll
#else
#  define POLL_FN ::poll
#endif

namespace caf::net {

namespace {

bool connect_with_timeout(stream_socket fd, const sockaddr* addr,
                          socklen_t addrlen, timespan timeout) {
  namespace sc = std::chrono;
  auto exit_guard = log::net::trace("fd.id = {}, timeout = {}", fd.id, timeout);
  // Set to non-blocking or fail.
  if (auto err = nonblocking(fd, true))
    return false;
  // Calculate deadline and define a lambda for getting the relative time in ms.
  auto deadline = sc::steady_clock::now() + timeout;
  auto ms_until_deadline = [deadline] {
    auto t = sc::steady_clock::now();
    auto ms_count = sc::duration_cast<sc::milliseconds>(deadline - t).count();
    return std::max(static_cast<int>(ms_count), 0);
  };
  // Call connect() once and see if it succeeds. Otherwise enter a poll()-loop.
  if (connect(fd.id, addr, addrlen) == 0) {
    // Done! Try restoring the socket to blocking and return.
    if (auto err = nonblocking(fd, false))
      return false;
    else
      return true;
  } else if (!last_socket_error_is_temporary()) {
    // Hard error. No need to restore the socket to blocking since we are going
    // to close it.
    return false;
  } else {
    // Loop until the reaching the deadline.
    pollfd pollset[1];
    pollset[0].fd = fd.id;
    pollset[0].events = POLLOUT;
    auto ms = ms_until_deadline();
    do {
      auto pres = POLL_FN(pollset, 1, ms);
      if (pres > 0) {
        // Check that the socket really is ready to go by reading SO_ERROR.
        if (probe(fd)) {
          // Done! Try restoring the socket to blocking and return.
          if (auto err = nonblocking(fd, false))
            return false;
          else
            return true;
        } else {
          return false;
        }
      } else if (pres < 0 && !last_socket_error_is_temporary()) {
        return false;
      }
      // Else: timeout or EINTR. Try-again.
      ms = ms_until_deadline();
    } while (ms > 0);
  }
  // No need to restore the socket to blocking since we are going to close it.
  return false;
}

template <int Family>
bool ip_connect(stream_socket fd, std::string host, uint16_t port,
                timespan timeout) {
  auto exit_guard = log::net::trace(
    "Family = {}, fd.id = {}, host = {}, port = {}, timeout = {}",
    (Family == AF_INET ? "AF_INET" : "AF_INET6"), fd.id, host, port, timeout);
  static_assert(Family == AF_INET || Family == AF_INET6, "invalid family");
  using sockaddr_type
    = std::conditional_t<Family == AF_INET, sockaddr_in, sockaddr_in6>;
  sockaddr_type sa;
  memset(&sa, 0, sizeof(sockaddr_type));
  if (inet_pton(Family, host.c_str(), &detail::addr_of(sa)) == 1) {
    detail::family_of(sa) = Family;
    detail::port_of(sa) = htons(port);
    using sa_ptr = const sockaddr*;
    if (timeout == infinite) {
      return ::connect(fd.id, reinterpret_cast<sa_ptr>(&sa), sizeof(sa)) == 0;
    } else {
      return connect_with_timeout(fd, reinterpret_cast<sa_ptr>(&sa), sizeof(sa),
                                  timeout);
    }
  } else {
    log::net::debug("inet_pton failed to parse {} for family", host,
                    (Family == AF_INET ? "AF_INET" : "AF_INET6"));
    return false;
  }
}

} // namespace

expected<tcp_stream_socket> make_connected_tcp_stream_socket(ip_endpoint node,
                                                             timespan timeout) {
  auto exit_guard = log::net::trace("node = {}, timeout = {}", node, timeout);
  if (timeout == infinite)
    log::net::debug("try to connect to TCP node {}", node);
  else
    log::net::debug("try to connect to TCP node {} with timeout {}", node,
                    timeout);
  auto proto = node.address().embeds_v4() ? AF_INET : AF_INET6;
  int socktype = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
  socktype |= SOCK_CLOEXEC;
#endif
  CAF_NET_SYSCALL("socket", fd, ==, -1, ::socket(proto, socktype, 0));
  tcp_stream_socket sock{fd};
  if (auto err = child_process_inherit(sock, false))
    return err;
  auto sguard = make_socket_guard(sock);
  if (proto == AF_INET6) {
    if (ip_connect<AF_INET6>(sock, to_string(node.address()), node.port(),
                             timeout)) {
      log::net::info("established TCP connection to IPv6 node {}",
                     to_string(node));
      return sguard.release();
    }
  } else if (ip_connect<AF_INET>(sock, to_string(node.address().embedded_v4()),
                                 node.port(), timeout)) {
    log::net::info("established TCP connection to IPv4 node {}",
                   to_string(node));
    return sguard.release();
  }
  log::net::info("failed to connect to {}", node);
  return make_error(sec::cannot_connect_to_node);
}

expected<tcp_stream_socket>
make_connected_tcp_stream_socket(const uri::authority_type& node,
                                 timespan timeout) {
  auto exit_guard = log::net::trace("mode = {}, timeout = {}", node, timeout);
  auto port = node.port;
  if (port == 0)
    return make_error(sec::cannot_connect_to_node, "port is zero");
  std::vector<ip_address> addrs;
  if (auto str = std::get_if<std::string>(&node.host))
    addrs = ip::resolve(*str);
  else if (auto addr = std::get_if<ip_address>(&node.host))
    addrs.push_back(*addr);
  if (addrs.empty())
    return make_error(sec::cannot_connect_to_node, "empty authority");
  for (auto& addr : addrs) {
    auto ep = ip_endpoint{addr, port};
    if (auto sock = make_connected_tcp_stream_socket(ep, timeout))
      return *sock;
  }
  return make_error(sec::cannot_connect_to_node, to_string(node));
}

expected<tcp_stream_socket> make_connected_tcp_stream_socket(std::string host,
                                                             uint16_t port,
                                                             timespan timeout) {
  auto exit_guard = log::net::trace("host = {}, port = {}, timeout = {}", host,
                                    port, timeout);
  uri::authority_type auth;
  auth.host = std::move(host);
  auth.port = port;
  return make_connected_tcp_stream_socket(auth, timeout);
}

} // namespace caf::net

namespace caf::detail {

expected<net::tcp_stream_socket>
tcp_try_connect(const uri::authority_type& auth, timespan connection_timeout,
                size_t max_retry_count, timespan retry_delay) {
  auto result = net::make_connected_tcp_stream_socket(auth, connection_timeout);
  if (result)
    return result;
  for (size_t i = 1; i <= max_retry_count; ++i) {
    std::this_thread::sleep_for(retry_delay);
    result = net::make_connected_tcp_stream_socket(auth, connection_timeout);
    if (result)
      return result;
  }
  return result;
}

expected<net::tcp_stream_socket>
tcp_try_connect(std::string host, uint16_t port, timespan connection_timeout,
                size_t max_retry_count, timespan retry_delay) {
  uri::authority_type auth;
  auth.host = std::move(host);
  auth.port = port;
  return tcp_try_connect(auth, connection_timeout, max_retry_count,
                         retry_delay);
}

} // namespace caf::detail
