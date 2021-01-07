// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>

#include "caf/config.hpp"

namespace caf {

/// Base type for addresses based on a byte representation such as IP or
/// Ethernet addresses.
template <class Derived>
class byte_address {
public:
  // -- element access ---------------------------------------------------------

  /// Returns the byte at given index.
  uint8_t& operator[](size_t index) noexcept {
    return dref().bytes()[index];
  }

  /// Returns the byte at given index.
  const uint8_t& operator[](size_t index) const noexcept {
    return dref().bytes()[index];
  }

  // -- properties -------------------------------------------------------------

  /// Returns the number of bytes of the address.
  size_t size() const noexcept {
    return dref().bytes().size();
  }

  // -- transformations --------------------------------------------------------

  Derived network_address(size_t prefix_length) const noexcept {
    static constexpr uint8_t netmask_tbl[] = {0x00, 0x80, 0xC0, 0xE0,
                                              0xF0, 0xF8, 0xFC, 0xFE};
    prefix_length = std::min(prefix_length, Derived::num_bytes * 8);
    Derived netmask;
    auto bytes_to_keep = prefix_length / 8;
    auto remainder = prefix_length % 8;
    size_t i = 0;
    for (; i < bytes_to_keep; ++i)
      netmask[i] = 0xFF;
    if (remainder != 0)
      netmask[i] = netmask_tbl[remainder];
    Derived result{dref()};
    result &= netmask;
    return result;
  }

  // -- bitwise member operators -----------------------------------------------

  /// Bitwise ANDs `*this` and `other`.
  Derived& operator&=(const Derived& other) {
    auto& buf = dref().bytes();
    for (size_t index = 0; index < Derived::num_bytes; ++index)
      buf[index] &= other[index];
    return dref();
  }

  /// Bitwise ORs `*this` and `other`.
  Derived& operator|=(const Derived& other) {
    auto& buf = dref().bytes();
    for (size_t index = 0; index < Derived::num_bytes; ++index)
      buf[index] |= other[index];
    return dref();
  }

  /// Bitwise XORs `*this` and `other`.
  Derived& operator^=(const Derived& other) {
    auto& buf = dref().bytes();
    for (size_t index = 0; index < Derived::num_bytes; ++index)
      buf[index] ^= other[index];
    return dref();
  }

  // -- bitwise free operators -------------------------------------------------

  friend Derived operator&(const Derived& x, const Derived& y) {
    Derived result{x};
    result &= y;
    return result;
  }

  friend Derived operator|(const Derived& x, const Derived& y) {
    Derived result{x};
    result |= y;
    return result;
  }

  friend Derived operator^(const Derived& x, const Derived& y) {
    Derived result{x};
    result ^= y;
    return result;
  }

private:
  Derived& dref() noexcept {
    return *static_cast<Derived*>(this);
  }

  const Derived& dref() const noexcept {
    return *static_cast<const Derived*>(this);
  }
};

} // namespace caf
