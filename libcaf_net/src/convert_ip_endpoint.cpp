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

namespace caf {
namespace detail {

sockaddr_in6 to_sockaddr(const ipv6_endpoint& ep) {
  sockaddr_in6 addr = {};
  addr.sin6_family = AF_INET6;
  addr.sin6_port = ntohs(ep.port());
  memcpy(&addr.sin6_addr, ep.address().bytes().data(), ep.address().size());
  return addr;
}

ipv6_endpoint to_ip_endpoint(const sockaddr_in6& addr) {
  ip_endpoint ep;
  ep.port(htons(addr.sin6_port));
  ipv6_address ip_addr;
  memcpy(ip_addr.bytes().data(), &addr.sin6_addr, ip_addr.size());
  ep.address(ip_addr);
  return ep;
}

sockaddr_in to_sockaddr(const ipv4_endpoint& ep) {
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = ntohs(ep.port());
  memcpy(&addr.sin_addr, ep.address().bytes().data(), ep.address().size());
  return addr;
}

ipv4_endpoint to_ip_endpoint(const sockaddr_in& addr) {
  ipv4_endpoint ep;
  ep.port(htons(addr.sin_port));
  ipv4_address ip_addr;
  memcpy(ip_addr.bytes().data(), &addr.sin_addr, ip_addr.size());
  ep.address(ip_addr);
  return ep;
}

} // namespace detail
} // namespace caf
