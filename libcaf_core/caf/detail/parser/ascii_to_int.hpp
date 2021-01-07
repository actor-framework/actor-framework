
// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail::parser {

template <int Base, class T>
struct ascii_to_int {
  // @pre `c` is a valid ASCII digit, i.e., matches [0-9].
  constexpr T operator()(char c) const {
    // Result is guaranteed to have a value between 0 and 10 and can be safely
    // cast to any integer type.
    return static_cast<T>(c - '0');
  }
};

template <class T>
struct ascii_to_int<16, T> {
  // @pre `c` is a valid ASCII hex digit, i.e., matches [0-9A-Fa-f].
  constexpr T operator()(char c) const {
    // Numbers start at position 48 in the ASCII table.
    // Uppercase characters start at position 65 in the ASCII table.
    // Lowercase characters start at position 97 in the ASCII table.
    // Result is guaranteed to have a value between 0 and 16 and can be safely
    // cast to any integer type.
    return static_cast<T>(
      c <= '9' ? c - '0' : (c <= 'F' ? 10 + (c - 'A') : 10 + (c - 'a')));
  }
};

} // namespace caf::detail::parser
