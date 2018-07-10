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
#include <cstring>

#include "caf/config.hpp"
#include "caf/detail/comparable.hpp"

namespace caf {

/// Base type for addresses based on a byte representation such as IP or
/// Ethernet addresses.
template <class Derived>
class byte_address : detail::comparable<Derived> {
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

  // -- comparison -------------------------------------------------------------

  /// Returns a negative number if `*this < other`, zero if `*this == other`
  /// and a positive number if `*this > other`.
  int compare(const Derived& other) const noexcept {
    auto& buf = dref().bytes();
    return memcmp(buf.data(), other.bytes().data(), Derived::num_bytes);
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
