// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/rfc3629.hpp"

namespace {

// Convenient literal for std::byte.
constexpr std::byte operator""_b(unsigned long long x) {
  return static_cast<std::byte>(x);
}

// Takes the first N bits of `value`.
template <size_t N>
constexpr std::byte head(std::byte value) noexcept {
  if constexpr (N == 1) {
    return value & 0b1000'0000_b;
  } else if constexpr (N == 2) {
    return value & 0b1100'0000_b;
  } else if constexpr (N == 3) {
    return value & 0b1110'0000_b;
  } else if constexpr (N == 4) {
    return value & 0b1111'0000_b;
  } else if constexpr (N == 5) {
    return value & 0b1111'1000_b;
  } else if constexpr (N == 6) {
    return value & 0b1111'1100_b;
  } else {
    static_assert(N == 7);
    return value & 0b1111'1110_b;
  }
}

// Takes the last N bits of `value`.
template <size_t N>
constexpr std::byte tail(std::byte value) noexcept {
  if constexpr (N == 1) {
    return value & 0b0000'0001_b;
  } else if constexpr (N == 2) {
    return value & 0b0000'0011_b;
  } else if constexpr (N == 3) {
    return value & 0b0000'0111_b;
  } else if constexpr (N == 4) {
    return value & 0b0000'1111_b;
  } else if constexpr (N == 5) {
    return value & 0b0001'1111_b;
  } else if constexpr (N == 6) {
    return value & 0b0011'1111_b;
  } else {
    static_assert(N == 7);
    return value & 0b0111'1111_b;
  }
}

// Checks whether `value` is an UTF-8 continuation byte.
constexpr bool is_continuation_byte(std::byte value) noexcept {
  return head<2>(value) == 0b1000'0000_b;
}

// The following code is based on the algorithm described in
// http://unicode.org/mail-arch/unicode-ml/y2003-m02/att-0467/01-The_Algorithm_to_Valide_an_UTF-8_String
bool validate_rfc3629(const std::byte* first, const std::byte* last) {
  while (first != last) {
    auto x = *first++;
    // First bit is zero: ASCII character.
    if (head<1>(x) == 0b0000'0000_b)
      continue;
    // 110b'xxxx: 2-byte sequence.
    if (head<3>(x) == 0b1100'0000_b) {
      // No non-shortest form.
      if (head<7>(x) == 0b1100'0000_b)
        return false;
      if (first == last || !is_continuation_byte(*first++))
        return false;
      continue;
    }
    // 1110'xxxx: 3-byte sequence.
    if (head<4>(x) == 0b1110'0000_b) {
      if (first == last || !is_continuation_byte(*first))
        return false;
      // No non-shortest form.
      if (x == 0b1110'0000_b && head<3>(*first) == 0b1000'0000_b)
        return false;
      // No surrogate characters.
      if (x == 0b1110'1101_b && head<3>(*first) == 0b1010'0000_b)
        return false;
      ++first;
      if (first == last || !is_continuation_byte(*first++))
        return false;
      continue;
    }
    // 1111'0xxx: 4-byte sequence.
    if (head<5>(x) == 0b1111'0000_b) {
      uint64_t code_point = std::to_integer<uint64_t>(tail<3>(x)) << 18;
      if (first == last || !is_continuation_byte(*first))
        return false;
      // No non-shortest form.
      if (x == 0b1111'0000_b && head<4>(*first) == 0b1000'0000_b)
        return false;
      code_point |= std::to_integer<uint64_t>(tail<6>(*first++)) << 12;
      if (first == last || !is_continuation_byte(*first))
        return false;
      code_point |= std::to_integer<uint64_t>(tail<6>(*first++)) << 6;
      if (first == last || !is_continuation_byte(*first))
        return false;
      code_point |= std::to_integer<uint64_t>(tail<6>(*first++));
      // Out of valid UTF range
      if (code_point >= 0x110000) {
        return false;
      }
      continue;
    }
    return false;
  }
  return true;
}

} // namespace

namespace caf::detail {

bool rfc3629::valid(const_byte_span bytes) noexcept {
  return validate_rfc3629(bytes.begin(), bytes.end());
}

} // namespace caf::detail
