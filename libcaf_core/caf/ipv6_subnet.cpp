// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/ipv6_subnet.hpp"

#include "caf/detail/mask_bits.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators --------------------

ipv6_subnet::ipv6_subnet(ipv4_subnet subnet) noexcept
  : address_(ipv6_address{subnet.network_address()}),
    prefix_length_(v4_offset + subnet.prefix_length()) {
  detail::mask_bits(address_.bytes(), prefix_length_);
}

ipv6_subnet::ipv6_subnet(ipv4_address addr, uint8_t len) noexcept
  : address_(addr), prefix_length_(len + v4_offset) {
  detail::mask_bits(address_.bytes(), prefix_length_);
}

ipv6_subnet::ipv6_subnet(ipv6_address addr, uint8_t len) noexcept
  : address_(addr), prefix_length_(len) {
  detail::mask_bits(address_.bytes(), prefix_length_);
}

// -- properties ---------------------------------------------------------------

bool ipv6_subnet::embeds_v4() const noexcept {
  return prefix_length_ >= v4_offset && address_.embeds_v4();
}

ipv4_subnet ipv6_subnet::embedded_v4() const noexcept {
  return {address_.embedded_v4(),
          static_cast<uint8_t>(prefix_length_ - v4_offset)};
}

bool ipv6_subnet::contains(ipv6_address addr) const noexcept {
  return address_ == addr.network_address(prefix_length_);
}

bool ipv6_subnet::contains(ipv6_subnet other) const noexcept {
  // We can only contain a subnet if it's prefix is greater or equal.
  if (prefix_length_ > other.prefix_length_)
    return false;
  return prefix_length_ == other.prefix_length_
           ? address_ == other.address_
           : address_ == other.address_.network_address(prefix_length_);
}

bool ipv6_subnet::contains(ipv4_address addr) const noexcept {
  return embeds_v4() ? embedded_v4().contains(addr) : false;
}

bool ipv6_subnet::contains(ipv4_subnet other) const noexcept {
  return embeds_v4() ? embedded_v4().contains(other) : false;
}

std::string to_string(ipv6_subnet x) {
  if (x.embeds_v4())
    return to_string(x.embedded_v4());
  auto result = to_string(x.network_address());
  result += '/';
  result += std::to_string(x.prefix_length());
  return result;
}

} // namespace caf
