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

#include <array>
#include <cstdint>
#include <string>

#include "caf/byte_address.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/fwd.hpp"

namespace caf {

class ipv4_address : public byte_address<ipv4_address> {
public:
  // -- constants --------------------------------------------------------------

  static constexpr size_t num_bytes = 4;

  // -- member types -----------------------------------------------------------

  using super = byte_address<ipv4_address>;

  using array_type = std::array<uint8_t, num_bytes>;

  // -- constructors, destructors, and assignment operators --------------------

  ipv4_address();

  explicit ipv4_address(array_type bytes);

  /// Constructs an IPv4 address from bits in network byte order.
  static ipv4_address from_bits(uint32_t bits) {
    ipv4_address result;
    result.bits(bits);
    return result;
  }

  // -- properties -------------------------------------------------------------

  /// Returns whether this is a loopback address.
  bool is_loopback() const noexcept;

  /// Returns whether this is a multicast address.
  bool is_multicast() const noexcept;

  /// Returns the bits of the IP address in a single integer arranged in network
  /// byte order.
  inline uint32_t bits() const noexcept {
    return bits_;
  }

  /// Sets all bits of the IP address with a single 32-bit write. Expects
  /// argument in network byte order.
  inline void bits(uint32_t value) noexcept {
    bits_ = value;
  }

  /// Returns the bytes of the IP address as array.
  inline array_type& bytes() noexcept {
    return bytes_;
  }

  /// Returns the bytes of the IP address as array.
  inline const array_type& bytes() const noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  inline array_type& data() noexcept {
    return bytes_;
  }

  /// Alias for `bytes()`.
  inline const array_type& data() const noexcept {
    return bytes_;
  }

  // -- inspection -------------------------------------------------------------

  template <class Inspector>
  friend typename Inspector::result_type inspect(Inspector& f, ipv4_address& x) {
    return f(x.bits_);
  }

private:
  // -- member variables -------------------------------------------------------

  union {
    uint32_t bits_;
    array_type bytes_;
  };

  // -- sanity checks ----------------------------------------------------------

  static_assert(sizeof(array_type) == sizeof(uint32_t),
                "array<char, 4> has a different size than uint32");
};

// -- related free functions ---------------------------------------------------

/// Convenience function for creating an IPv4 address from octets.
/// @relates ipv4_address
ipv4_address make_ipv4_address(uint8_t oct1, uint8_t oct2, uint8_t oct3,
                               uint8_t oct4);

/// Returns a human-readable string representation of the address.
/// @relates ipv4_address
std::string to_string(const ipv4_address& x);

/// Tries to parse the content of `str` into `dest`.
/// @relates ipv4_address
error parse(string_view str, ipv4_address& dest);

} // namespace caf
