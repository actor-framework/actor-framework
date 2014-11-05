/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_ATOM_HPP
#define CAF_ATOM_HPP

#include <string>

#include "caf/detail/atom_val.hpp"

namespace caf {

/**
 * The value type of atoms.
 */
enum class atom_value : uint64_t {
  /** @cond PRIVATE */
  dirty_little_hack = 31337
  /** @endcond */

};

/**
 * Creates an atom from given string literal.
 */
template <size_t Size>
constexpr atom_value atom(char const (&str)[Size]) {
  // last character is the NULL terminator
  static_assert(Size <= 11, "only 10 characters are allowed");
  return static_cast<atom_value>(detail::atom_val(str, 0xF));
}

} // namespace caf

#endif // CAF_ATOM_HPP
