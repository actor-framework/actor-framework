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

#include "caf/ipv6_subnet.hpp"

#include "caf/detail/mask_bits.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators --------------------

ipv6_subnet::ipv6_subnet() : prefix_length_(0) {
  // nop
}

ipv6_subnet::ipv6_subnet(ipv4_subnet subnet)
    : address_(ipv6_address{subnet.network_address()}),
      prefix_length_(v4_offset + subnet.prefix_length()){
  detail::mask_bits(address_.bytes(), prefix_length_);
}

ipv6_subnet::ipv6_subnet(ipv4_address network_address, uint8_t prefix_length)
    : address_(network_address),
      prefix_length_(prefix_length + v4_offset) {
  detail::mask_bits(address_.bytes(), prefix_length_);
}

ipv6_subnet::ipv6_subnet(ipv6_address network_address, uint8_t prefix_length)
    : address_(network_address),
      prefix_length_(prefix_length) {
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

// -- comparison ---------------------------------------------------------------

int ipv6_subnet::compare(const ipv6_subnet& other) const noexcept {
  auto sub_res = address_.compare(other.address_);
  return sub_res != 0 ? sub_res
                      : static_cast<int>(prefix_length_) - other.prefix_length_;
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

