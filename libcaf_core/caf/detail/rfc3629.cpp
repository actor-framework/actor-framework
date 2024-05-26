// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

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

// Checks whether `value` is an UTF-8 continuation byte.
constexpr bool is_continuation_byte(std::byte value) noexcept {
  return head<2>(value) == 0b1000'0000_b;
}

// The following code is based on the algorithm described in
// http://unicode.org/mail-arch/unicode-ml/y2003-m02/att-0467/01-The_Algorithm_to_Valide_an_UTF-8_String
// Returns a pair consisting of an iterator to the of the valid  range, and a
// boolean stating whether the validation failed because of incomplete data, or
// other failures like malformed encoding or invalid code point.
std::pair<const std::byte*, bool> validate_rfc3629(const std::byte* first,
                                                   const std::byte* last) {
  while (first != last) {
    auto checkpoint = first;
    auto x = *first++;
    // First bit is zero: ASCII character.
    if (head<1>(x) == 0b0000'0000_b)
      continue;
    // 110b'xxxx: 2-byte sequence.
    if (head<3>(x) == 0b1100'0000_b) {
      // No non-shortest form.
      if (head<7>(x) == 0b1100'0000_b)
        return {checkpoint, false};
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first++))
        return {checkpoint, false};
      continue;
    }
    // 1110'xxxx: 3-byte sequence.
    if (head<4>(x) == 0b1110'0000_b) {
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first))
        return {checkpoint, false};
      // No non-shortest form.
      if (x == 0b1110'0000_b && head<3>(*first) == 0b1000'0000_b)
        return {checkpoint, false};
      // No surrogate characters.
      if (x == 0b1110'1101_b && head<3>(*first) == 0b1010'0000_b)
        return {checkpoint, false};
      ++first;
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first++))
        return {checkpoint, false};
      continue;
    }
    // 1111'0xxx: 4-byte sequence.
    if (head<5>(x) == 0b1111'0000_b) {
      // Check if code point is in the valid UTF range.
      if (x > std::byte{0xf4})
        return {checkpoint, false};
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first))
        return {checkpoint, false};
      // No non-shortest form.
      if (x == 0b1111'0000_b && head<4>(*first) == 0b1000'0000_b)
        return {checkpoint, false};
      // Check if code point is in the valid UTF range.
      if (x == std::byte{0xf4} && *first >= std::byte{0x90})
        return {checkpoint, false};
      ++first;
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first++))
        return {checkpoint, false};
      if (first == last)
        return {checkpoint, true};
      if (!is_continuation_byte(*first++))
        return {checkpoint, false};
      continue;
    }
    return {checkpoint, false};
  }
  return {last, false};
}

} // namespace

namespace caf::detail {

bool rfc3629::valid(const_byte_span bytes) noexcept {
  return validate_rfc3629(bytes.begin(), bytes.end()).first == bytes.end();
}

std::pair<size_t, bool> rfc3629::validate(const_byte_span bytes) noexcept {
  auto [last, incomplete] = validate_rfc3629(bytes.begin(), bytes.end());
  return {static_cast<size_t>(last - bytes.begin()), incomplete};
}

} // namespace caf::detail
