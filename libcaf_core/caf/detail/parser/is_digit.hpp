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
