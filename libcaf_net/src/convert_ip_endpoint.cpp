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

#include "caf/detail/convert_ip_endpoint.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv6_endpoint.hpp"
#include <iostream>

namespace caf {
namespace detail {

sockaddr_storage to_sockaddr(const ip_endpoint& ep) {
  sockaddr_storage sockaddr = {};
  if (ep.address().embeds_v4()) {
    auto sockaddr4 = reinterpret_cast<sockaddr_in*>(&sockaddr);
    sockaddr4->sin_family = AF_INET;
    sockaddr4->sin_port = ntohs(ep.port());
    sockaddr4->sin_addr.s_addr = ep.address().embedded_v4().bits();
  } else {
    auto sockaddr6 = reinterpret_cast<sockaddr_in6*>(&sockaddr);
    sockaddr6->sin6_family = AF_INET6;
    sockaddr6->sin6_port = ntohs(ep.port());
    memcpy(&sockaddr6->sin6_addr, ep.address().bytes().data(),
           ep.address().bytes().size());
  }
  return sockaddr;
}

ip_endpoint to_ip_endpoint(const sockaddr_storage& addr) {
  if (addr.ss_family == AF_INET) {
    auto sockaddr4 = reinterpret_cast<const sockaddr_in*>(&addr);
    ipv4_address ipv4_addr;
    memcpy(ipv4_addr.data().data(), &sockaddr4->sin_addr, ipv4_addr.size());
    return {ipv4_addr, htons(sockaddr4->sin_port)};
  } else {
    auto sockaddr6 = reinterpret_cast<const sockaddr_in6*>(&addr);
    ipv6_address ipv6_addr;
    memcpy(ipv6_addr.bytes().data(), &sockaddr6->sin6_addr,
           ipv6_addr.bytes().size());
    return {ipv6_addr, htons(sockaddr6->sin6_port)};
  }
}

} // namespace detail
} // namespace caf
