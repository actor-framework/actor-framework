// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/ipv6_endpoint.hpp"

#include "caf/hash/fnv.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"

namespace caf {

ipv6_endpoint::ipv6_endpoint(ipv6_address address, uint16_t port)
  : address_(address), port_(port) {
  // nop
}

ipv6_endpoint::ipv6_endpoint(ipv4_address address, uint16_t port)
  : address_(address), port_(port) {
  // nop
}

size_t ipv6_endpoint::hash_code() const noexcept {
  return hash::fnv<size_t>::compute(address_, port_);
}

long ipv6_endpoint::compare(ipv6_endpoint x) const noexcept {
  auto res = address_.compare(x.address());
  return res == 0 ? port_ - x.port() : res;
}

long ipv6_endpoint::compare(ipv4_endpoint x) const noexcept {
  ipv6_endpoint y{x.address(), x.port()};
  return compare(y);
}

std::string to_string(const ipv6_endpoint& x) {
  std::string result;
  auto addr = x.address();
  if (addr.embeds_v4()) {
    result += to_string(addr);
    result += ":";
    result += std::to_string(x.port());
  } else {
    result += '[';
    result += to_string(addr);
    result += "]:";
    result += std::to_string(x.port());
  }
  return result;
}

} // namespace caf
