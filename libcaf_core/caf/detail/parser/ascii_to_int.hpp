
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

template <int Base, class T>
struct ascii_to_int {
  // @pre `c` is a valid ASCII digit, i.e., matches [0-9].
  constexpr T operator()(char c) const {
    return c - '0';
  }
};

template <class T>
struct ascii_to_int<16, T> {
  // @pre `c` is a valid ASCII hex digit, i.e., matches [0-9A-Fa-f].
  constexpr T operator()(char c) const {
    // Numbers start at position 48 in the ASCII table.
    // Uppercase characters start at position 65 in the ASCII table.
    // Lowercase characters start at position 97 in the ASCII table.
    return c <= '9' ? c - '0' : (c <= 'F' ? 10 + (c - 'A') : 10 + (c - 'a'));
  }
};

} // namespace parser
} // namespace detail
} // namespace caf
