// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/ipv4_address.hpp"

#include <cstdint>

namespace caf {

class CAF_CORE_EXPORT ipv4_subnet : detail::comparable<ipv4_subnet> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  ipv4_subnet();

  ipv4_subnet(const ipv4_subnet&) = default;

  ipv4_subnet(ipv4_address network_address, uint8_t prefix_length);

  ipv4_subnet& operator=(const ipv4_subnet&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns the network address for this subnet.
  const ipv4_address& network_address() const noexcept {
    return address_;
  }

  /// Returns the prefix length of the netmask in bits.
  uint8_t prefix_length() const noexcept {
    return prefix_length_;
  }

  /// Returns whether `addr` belongs to this subnet.
  bool contains(ipv4_address addr) const noexcept;

  /// Returns whether this subnet includes `other`.
  bool contains(ipv4_subnet other) const noexcept;

  // -- comparison -------------------------------------------------------------

  int compare(const ipv4_subnet& other) const noexcept;

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv4_subnet& x) {
    return f.object(x).fields(f.field("address", x.address_),
                              f.field("prefix_length", x.prefix_length_));
  }

private:
  // -- member variables -------------------------------------------------------

  ipv4_address address_;
  uint8_t prefix_length_;
};

// -- related free functions ---------------------------------------------------

/// @relates ipv4_subnet
CAF_CORE_EXPORT std::string to_string(ipv4_subnet x);

} // namespace caf
