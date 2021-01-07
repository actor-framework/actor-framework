// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

namespace caf::detail::parser {

/// Returns whether `c` equals `C`.
template <char C>
bool is_char(char c) {
  return c == C;
}

/// Returns whether `c` equals `C` (case insensitive).
template <char C>
bool is_ichar(char c) {
  static_assert(C >= 'a' && C <= 'z', "Expected a-z (lowercase).");
  return c == C || c == (C - 32);
}

} // namespace caf::detail::parser
