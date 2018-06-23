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

namespace caf {
namespace detail {
namespace parser {

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

} // namespace parser
} // namespace detail
} // namespace caf


