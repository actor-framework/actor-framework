// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv4_endpoint.hpp"

#include "caf/hash/fnv.hpp"

namespace caf {

size_t ipv4_endpoint::hash_code() const noexcept {
  return hash::fnv<size_t>::compute(address_, port_);
}

std::string to_string(const ipv4_endpoint& ep) {
  return to_string(ep.address()) + ":" + std::to_string(ep.port());
}

} // namespace caf
