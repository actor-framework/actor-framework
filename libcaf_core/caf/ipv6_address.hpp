// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "caf/byte_address.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/ipv4_address.hpp"

namespace caf {

class CAF_CORE_EXPORT ipv6_address
  : public byte_address<ipv6_address>,
    detail::comparable<ipv6_address>,
    detail::comparable<ipv6_address, ipv4_address> {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t num_bytes = 16;

  // -- member types -----------------------------------------------------------

  using super = byte_address<ipv6_address>;

  using array_type = std::array<uint8_t, num_bytes>;

  using u16_array_type = std::array<uint16_t, 8>;

  using uint16_ilist = std::initializer_list<uint16_t>;

  // -- constructors, destructors, and assignment operators --------------------

  /// Constructs an all-zero address.
  ipv6_address();

  /// Constructs an address from given prefix and suffix.
  /// @pre `prefix.size() + suffix.size() <= 8`
  /// @warning assumes network byte order for prefix and suffix
  ipv6_address(uint16_ilist prefix, uint16_ilist suffix);

  /// Embeds an IPv4 address into an IPv6 address.
  explicit ipv6_address(ipv4_address addr);

  /// Constructs an IPv6 address from given bytes.
  explicit ipv6_address(array_type bytes);

  // -- comparison -------------------------------------------------------------

  /// Returns a negative number if `*this < other`, zero if `*this == other`
  /// and a positive number if `*this > other`.
  int compare(ipv6_address other) const noexcept;

  /// Returns a negative number if `*this < other`, zero if `*this == other`
  /// and a positive number if `*this > other`.
  int compare(ipv4_address other) const noexcept;

  // -- properties -------------------------------------------------------------

  /// Returns whether this address embeds an IPv4 address.
  bool embeds_v4() const noexcept;

  /// Returns an embedded IPv4 address.
  /// @pre `embeds_v4()`
  ipv4_address embedded_v4() const noexcept;

  /// Returns whether this is a loopback address.
  bool is_loopback() const noexcept;

  /// Returns the bytes of the IP address as array.
  array_type& bytes() noexcept {
    return bytes_;
  }

  /// Returns the bytes of the IP address as array.
  const array_type& bytes() const noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  array_type& data() noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  const array_type& data() const noexcept {
    return bytes_;
  }

  /// Returns whether this address contains only zeros, i.e., equals `::`.
  bool zero() const noexcept {
    return half_segments_[0] == 0 && half_segments_[1] == 0;
  }

  // -- factories --------------------------------------------------------------

  /// Returns `INADDR6_ANY`, i.e., `::`.
  static ipv6_address any() noexcept;

  /// Returns `INADDR6_LOOPBACK`, i.e., `::1`.
  static ipv6_address loopback() noexcept;

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend bool inspect(Inspector& f, ipv6_address& x) {
    return f.object(x).fields(f.field("bytes", x.bytes_));
  }

  friend CAF_CORE_EXPORT std::string to_string(ipv6_address x);

private:
  // -- member variables -------------------------------------------------------

  union {
    std::array<uint64_t, 2> half_segments_;
    std::array<uint32_t, 4> quad_segments_;
    u16_array_type oct_segments_;
    array_type bytes_;
  };
};

CAF_CORE_EXPORT error parse(string_view str, ipv6_address& dest);

} // namespace caf
