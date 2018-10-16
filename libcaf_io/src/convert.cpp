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

#include "caf/io/convert.hpp"

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
namespace io {

bool convert(const sockaddr& src, protocol::transport tp, ip_endpoint& dst) {
  if (src.sa_family == AF_INET) {
    auto& v4 = reinterpret_cast<const sockaddr_in&>(src);
    auto port = ntohs(v4.sin_port);
    ipv4_address::array_type bytes;
    static_assert(sizeof(in_addr) == ipv4_address::num_bytes,
                  "sizeof(in_addr) != ipv4_address::num_bytes");
    memcpy(bytes.data(), &v4.sin_addr, sizeof(in_addr));
    dst = ip_endpoint{ipv4_address{bytes}, port, tp};
    return true;
  }
  if (src.sa_family == AF_INET6) {
    auto& v6 = reinterpret_cast<const sockaddr_in6&>(src);
    auto port = ntohs(v6.sin6_port);
    ipv6_address::array_type bytes;
    static_assert(sizeof(in6_addr) == ipv6_address::num_bytes,
                  "sizeof(in6_addr) != ipv6_address::num_bytes");
    memcpy(bytes.data(), &v6.sin6_addr, sizeof(in6_addr));
    dst = ip_endpoint{ipv6_address{bytes}, port, tp};
    return true;
  }
  return false;
}

bool convert(const addrinfo& src, ip_endpoint& dst) {
  if (src.ai_addr == nullptr)
    return false;
  switch (src.ai_protocol) {
    default:
      return false;
    case IPPROTO_TCP:
      return convert(*src.ai_addr, protocol::tcp, dst);
    case IPPROTO_UDP:
      return convert(*src.ai_addr, protocol::udp, dst);
  }
}

bool convert(const ip_endpoint& src, sockaddr_storage& dst) {
  memset(&dst, 0, sizeof(sockaddr_storage));
  auto& addr = reinterpret_cast<sockaddr&>(dst);
  if (src.is_v4()) {
    addr.sa_family = AF_INET;
    auto& v4 = reinterpret_cast<sockaddr_in&>(addr);
    v4.sin_port = htons(src.port());
    auto v4_addr = src.address().embedded_v4();
    memcpy(&v4.sin_addr, v4_addr.bytes().data(), ipv4_address::num_bytes);
  } else {
    addr.sa_family = AF_INET6;
    auto& v6 = reinterpret_cast<sockaddr_in6&>(addr);
    v6.sin6_port = htons(src.port());
    memcpy(&v6.sin6_addr, src.address().bytes().data(),
           ipv6_address::num_bytes);
  }
  return true;
}

} // namespace io
} // namespace caf
