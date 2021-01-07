// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include <cstdint>
#include <type_traits>

#include "caf/detail/type_traits.hpp"

#pragma once

namespace caf {

/// A C++11/14 drop-in replacement for C++17's `std::byte`.
enum class byte : uint8_t {};

template <class IntegerType,
          class = detail::enable_if_tt<std::is_integral<IntegerType>>>
constexpr IntegerType to_integer(byte x) noexcept {
  return static_cast<IntegerType>(x);
}

template <class IntegerType,
          class E = detail::enable_if_tt<std::is_integral<IntegerType>>>
constexpr byte& operator<<=(byte& x, IntegerType shift) noexcept {
  return x = static_cast<byte>(to_integer<uint8_t>(x) << shift);
}

template <class IntegerType,
          class E = detail::enable_if_tt<std::is_integral<IntegerType>>>
constexpr byte operator<<(byte x, IntegerType shift) noexcept {
  return static_cast<byte>(to_integer<uint8_t>(x) << shift);
}

template <class IntegerType,
          class E = detail::enable_if_tt<std::is_integral<IntegerType>>>
constexpr byte& operator>>=(byte& x, IntegerType shift) noexcept {
  return x = static_cast<byte>(to_integer<uint8_t>(x) >> shift);
}

template <class IntegerType,
          class E = detail::enable_if_tt<std::is_integral<IntegerType>>>
constexpr byte operator>>(byte x, IntegerType shift) noexcept {
  return static_cast<byte>(static_cast<unsigned char>(x) >> shift);
}

inline byte& operator|=(byte& x, byte y) noexcept {
  return x = static_cast<byte>(to_integer<uint8_t>(x) | to_integer<uint8_t>(y));
}

constexpr byte operator|(byte x, byte y) noexcept {
  return static_cast<byte>(to_integer<uint8_t>(x) | to_integer<uint8_t>(y));
}

inline byte& operator&=(byte& x, byte y) noexcept {
  return x = static_cast<byte>(to_integer<uint8_t>(x) & to_integer<uint8_t>(y));
}

constexpr byte operator&(byte x, byte y) noexcept {
  return static_cast<byte>(to_integer<uint8_t>(x) & to_integer<uint8_t>(y));
}

inline byte& operator^=(byte& x, byte y) noexcept {
  return x = static_cast<byte>(to_integer<uint8_t>(x) ^ to_integer<uint8_t>(y));
}

constexpr byte operator^(byte x, byte y) noexcept {
  return static_cast<byte>(to_integer<uint8_t>(x) ^ to_integer<uint8_t>(y));
}

constexpr byte operator~(byte x) noexcept {
  return static_cast<byte>(~to_integer<uint8_t>(x));
}

} // namespace caf
