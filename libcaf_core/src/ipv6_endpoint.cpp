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

#include "caf/ipv6_endpoint.hpp"

#include "caf/detail/fnv_hash.hpp"

#ifdef CAF_WINDOWS
#  include <windows.h>
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <cerrno>
#  include <netinet/in.h>
#  include <netinet/ip.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

#ifdef CAF_WINDOWS
using sa_family_t = short;
#endif

using caf::detail::fnv_hash;
using caf::detail::fnv_hash_append;

namespace caf {

ipv6_endpoint::ipv6_endpoint(ipv6_address address, uint16_t port)
  : address_(address), port_(port) {
  // nop
}

ipv6_address ipv6_endpoint::address() const noexcept {
  return address_;
}

void ipv6_endpoint::address(ipv6_address x) noexcept {
  address_ = x;
}

uint16_t ipv6_endpoint::port() const noexcept {
  return port_;
}

void ipv6_endpoint::port(uint16_t x) noexcept {
  port_ = x;
}

size_t ipv6_endpoint::hash_code() const noexcept {
  auto result = fnv_hash(address_.data());
  return fnv_hash_append(result, port_);
}

long ipv6_endpoint::compare(ipv6_endpoint x) const noexcept {
  auto res = address_.compare(x.address());
  if (res != 0)
    return port_ - x.port();
  else
    return res;
}

std::string to_string(const ipv6_endpoint& ep) {
  return to_string(ep.address()) + ":" + std::to_string(ep.port());
}

} // namespace caf
