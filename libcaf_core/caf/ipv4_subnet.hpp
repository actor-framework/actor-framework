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

#pragma once

#include <cstdint>

#include "caf/detail/comparable.hpp"
#include "caf/ipv4_address.hpp"

namespace caf {

class ipv4_subnet : detail::comparable<ipv4_subnet> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  ipv4_subnet();

  ipv4_subnet(const ipv4_subnet&) = default;

  ipv4_subnet(ipv4_address network_address, uint8_t prefix_length);

  ipv4_subnet& operator=(const ipv4_subnet&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the network address for this subnet.
  inline const ipv4_address& network_address() const noexcept {
    return address_;
  }

  /// Returns the prefix length of the netmask in bits.
  inline uint8_t prefix_length() const noexcept {
    return prefix_length_;
  }

  /// Returns whether `addr` belongs to this subnet.
  bool contains(ipv4_address addr) const noexcept;

  /// Returns whether this subnet includes `other`.
  bool contains(ipv4_subnet other) const noexcept;

  // -- comparison -------------------------------------------------------------

  int compare(const ipv4_subnet& other) const noexcept;

private:
  // -- member variables -------------------------------------------------------

  ipv4_address address_;
  uint8_t prefix_length_;
};

// -- related free functions ---------------------------------------------------

/// @relates ipv4_subnet
std::string to_string(ipv4_subnet x);

} // namespace caf
