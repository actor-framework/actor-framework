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

#include "caf/detail/try_match.hpp"

#include "caf/type_erased_tuple.hpp"

namespace caf {
namespace detail {

using pattern_iterator = const meta_element*;

bool match_element(const meta_element& me, const type_erased_tuple& xs,
                   size_t pos) {
  CAF_ASSERT(me.typenr != 0 || me.type != nullptr);
  return xs.matches(pos, me.typenr, me.type);
}

bool match_atom_constant(const meta_element& me, const type_erased_tuple& xs,
                         size_t pos) {
  CAF_ASSERT(me.typenr == type_nr<atom_value>::value);
  if (!xs.matches(pos, type_nr<atom_value>::value, nullptr))
    return false;
  auto ptr = xs.get(pos);
  return me.v == *reinterpret_cast<const atom_value*>(ptr);
}

bool try_match(const type_erased_tuple& xs,
               pattern_iterator iter, size_t ps) {
  if (xs.size() != ps)
    return false;
  for (size_t i = 0; i < ps; ++i, ++iter)
    // inspect current element
    if (!iter->fun(*iter, xs, i))
      // type mismatch
      return false;
  return true;
}

} // namespace detail
} // namespace caf
