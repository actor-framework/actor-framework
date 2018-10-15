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

#include "caf/ip_endpoint.hpp"

#ifdef CAF_WINDOWS
# include <windows.h>
# include <winsock2.h>
# include <ws2ipdef.h>
# include <ws2tcpip.h>
#else
# include <arpa/inet.h>
# include <netdb.h>
# include <netinet/in.h>
# include <netinet/ip.h>
# include <sys/socket.h>
# include <unistd.h>
#endif

namespace caf {

ip_endpoint::ip_endpoint() : port_(0), transport_(protocol::tcp) {
  // nop
}

ip_endpoint::ip_endpoint(ip_address addr, uint16_t port, protocol::transport tp)
    : address_(addr),
      port_(port),
      transport_(tp) {
  // nop
}

ip_endpoint::ip_endpoint(ipv4_address addr, uint16_t port,
                         protocol::transport tp)
    : address_(addr),
      port_(port),
      transport_(tp) {
  // nop
}

int ip_endpoint::compare(const ip_endpoint& other) const noexcept {
  auto sub_res = address_.compare(other.address());
  auto compress = [](const ip_endpoint& x) {
    return (x.port_ << 1) | x.transport_;
  };
  return sub_res != 0 ? sub_res : compress(*this) - compress(other);
}

bool try_assign(ip_endpoint& x, const sockaddr& y, protocol::transport tp) {
  if (y.sa_family == AF_INET) {
    auto& v4 = reinterpret_cast<const sockaddr_in&>(y);
    auto port = ntohs(v4.sin_port);
    ipv4_address::array_type bytes;
    static_assert(sizeof(in_addr) == ipv4_address::num_bytes,
                  "sizeof(in_addr) != ipv4_address::num_bytes");
    memcpy(bytes.data(), &v4.sin_addr, sizeof(in_addr));
    x = ip_endpoint{ipv4_address{bytes}, port, tp};
    return true;
  }
  if (y.sa_family == AF_INET6) {
    auto& v6 = reinterpret_cast<const sockaddr_in6&>(y);
    auto port = ntohs(v6.sin6_port);
    ipv6_address::array_type bytes;
    static_assert(sizeof(in6_addr) == ipv6_address::num_bytes,
                  "sizeof(in6_addr) != ipv6_address::num_bytes");
    memcpy(bytes.data(), &v6.sin6_addr, sizeof(in6_addr));
    x = ip_endpoint{ipv6_address{bytes}, port, tp};
    return true;
  }
  return false;
}

bool try_assign(ip_endpoint& x, const addrinfo& y) {
  if (y.ai_addr == nullptr)
    return false;
  switch (y.ai_protocol) {
    default:
      return false;
    case IPPROTO_TCP:
      return try_assign(x, *y.ai_addr, protocol::tcp);
    case IPPROTO_UDP:
      return try_assign(x, *y.ai_addr, protocol::udp);
  }
}

void assign(sockaddr_storage& x, const ip_endpoint& y) {
  memset(&x, 0, sizeof(sockaddr_storage));
  if (y.is_v4()) {
    x.ss_len = sizeof(sockaddr_in);
    auto& v4 = reinterpret_cast<sockaddr_in&>(x);
    v4.sin_port = htons(y.port());
    auto v4_addr = y.address().embedded_v4();
    memcpy(&v4.sin_addr, v4_addr.bytes().data(), ipv4_address::num_bytes);
  } else {
    x.ss_len = sizeof(sockaddr_in6);
    auto& v6 = reinterpret_cast<sockaddr_in6&>(x);
    v6.sin6_port = htons(y.port());
    memcpy(&v6.sin6_addr, y.address().bytes().data(), ipv6_address::num_bytes);
  }
}

} // namespace caf
