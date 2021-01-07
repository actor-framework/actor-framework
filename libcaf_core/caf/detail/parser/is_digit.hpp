// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail::parser {

/// Returns whether `c` is a valid digit for a given base.
template <int Base>
constexpr bool is_digit(char c) noexcept {
  if constexpr (Base == 2) {
    return c == '0' || c == '1';
  } else if constexpr (Base == 8) {
    return c >= '0' && c <= '7';
  } else if constexpr (Base == 10) {
    return c >= '0' && c <= '9';
  } else {
    static_assert(Base == 16);
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
           || (c >= 'a' && c <= 'f');
  }
}

} // namespace caf::detail::parser
