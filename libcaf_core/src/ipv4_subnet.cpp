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

#include "caf/ipv4_subnet.hpp"

#include "caf/detail/mask_bits.hpp"

namespace caf {

// -- constructors, destructors, and assignment operators --------------------

ipv4_subnet::ipv4_subnet() : prefix_length_(0) {
  // nop
}

ipv4_subnet::ipv4_subnet(ipv4_address network_address, uint8_t prefix_length)
    : address_(network_address),
      prefix_length_(prefix_length) {
  detail::mask_bits(address_.bytes(), prefix_length_);
}

// -- properties ---------------------------------------------------------------

bool ipv4_subnet::contains(ipv4_address addr) const noexcept {
  return address_ == addr.network_address(prefix_length_);
}

bool ipv4_subnet::contains(ipv4_subnet other) const noexcept {
  // We can only contain a subnet if it's prefix is greater or equal.
  if (prefix_length_ > other.prefix_length_)
    return false;
  return prefix_length_ == other.prefix_length_
         ? address_ == other.address_
         : address_ == other.address_.network_address(prefix_length_);
}

// -- comparison ---------------------------------------------------------------

int ipv4_subnet::compare(const ipv4_subnet& other) const noexcept {
  auto sub_res = address_.compare(other.address_);
  return sub_res != 0 ? sub_res
                      : static_cast<int>(prefix_length_) - other.prefix_length_;
}

std::string to_string(ipv4_subnet x) {
  auto result = to_string(x.network_address());
  result += '/';
  result += std::to_string(x.prefix_length());
  return result;
}

} // namespace caf
