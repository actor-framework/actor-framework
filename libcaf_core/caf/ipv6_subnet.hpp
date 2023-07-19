// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_subnet.hpp"
#include "caf/ipv6_address.hpp"

#include <cstdint>

namespace caf {

class CAF_CORE_EXPORT ipv6_subnet : detail::comparable<ipv6_subnet> {
public:
  // -- constants --------------------------------------------------------------

  /// Stores the offset of an embedded IPv4 subnet in bits.
  static constexpr uint8_t v4_offset
    = static_cast<uint8_t>(ipv6_address::num_bytes - ipv4_address::num_bytes)
      * 8;

  // -- constructors, destructors, and assignment operators --------------------

  ipv6_subnet();

  ipv6_subnet(const ipv6_subnet&) = default;

  explicit ipv6_subnet(ipv4_subnet subnet);

  ipv6_subnet(ipv4_address network_address, uint8_t prefix_length);

  ipv6_subnet(ipv6_address network_address, uint8_t prefix_length);

  ipv6_subnet& operator=(const ipv6_subnet&) = default;

  // -- properties -------------------------------------------------------------

  /// Returns whether this subnet embeds an IPv4 subnet.
  bool embeds_v4() const noexcept;

  /// Returns an embedded IPv4 subnet.
  /// @pre `embeds_v4()`
  ipv4_subnet embedded_v4() const noexcept;

  /// Returns the network address for this subnet.
  const ipv6_address& network_address() const noexcept {
    return address_;
  }

  /// Returns the prefix length of the netmask.
  uint8_t prefix_length() const noexcept {
    return prefix_length_;
  }

  /// Returns whether `addr` belongs to this subnet.
  bool contains(ipv6_address addr) const noexcept;

  /// Returns whether this subnet includes `other`.
  bool contains(ipv6_subnet other) const noexcept;

  /// Returns whether `addr` belongs to this subnet.
  bool contains(ipv4_address addr) const noexcept;

  /// Returns whether this subnet includes `other`.
  bool contains(ipv4_subnet other) const noexcept;

  // -- comparison -------------------------------------------------------------

  int compare(const ipv6_subnet& other) const noexcept;

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv6_subnet& x) {
    return f.object(x).fields(f.field("address", x.address_),
                              f.field("prefix_length", x.prefix_length_));
  }

private:
  // -- member variables -------------------------------------------------------

  ipv6_address address_;
  uint8_t prefix_length_;
};

// -- related free functions ---------------------------------------------------

/// @relates ipv6_subnet
CAF_CORE_EXPORT std::string to_string(ipv6_subnet x);

} // namespace caf
