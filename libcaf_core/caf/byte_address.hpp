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

  // -- comparison -------------------------------------------------------------

  /// Returns a negative number if `*this < other`, zero if `*this == other`
  /// and a positive number if `*this > other`.
  int compare(const Derived& other) const noexcept {
    auto& buf = dref().bytes();
    return memcmp(buf.data(), other.bytes().data(), Derived::num_bytes);
  }

  // -- mutators ---------------------------------------------------------------

  /// Masks out lower bytes of the address. For example, calling `mask(1)` on
  /// the IPv4 address `192.168.1.1` would produce `192.0.0.0`.
  /// @pre `bytes_to_keep < num_bytes`
  void mask(int bytes_to_keep) noexcept {
    auto& buf = dref().bytes();
    memset(buf.data() + bytes_to_keep, 0, Derived::num_bytes - bytes_to_keep);
  }

  /// Returns a copy of this address with that masks out lower bytes. For
  /// example, calling `masked(1)` on the IPv4 address `192.168.1.1` would
  /// return `192.0.0.0`.
  /// @pre `bytes_to_keep < num_bytes`
  Derived masked(int bytes_to_keep) const noexcept {
    Derived result{dref()};
    result.mask(bytes_to_keep);
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
