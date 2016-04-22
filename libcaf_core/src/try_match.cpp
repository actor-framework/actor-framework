/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/detail/try_match.hpp"

namespace caf {
namespace detail {

using pattern_iterator = const meta_element*;

bool match_element(const meta_element& me, const type_erased_tuple* xs,
                   size_t pos, void** storage) {
  CAF_ASSERT(me.typenr != 0 || me.type != nullptr);
  if (! xs->matches(pos, me.typenr, me.type))
    return false;
  *storage = const_cast<void*>(xs->get(pos));
  return true;
}

bool match_atom_constant(const meta_element& me, const type_erased_tuple* xs,
                         size_t pos, void** storage) {
  CAF_ASSERT(me.typenr == detail::type_nr<atom_value>::value);
  if (! xs->matches(pos, detail::type_nr<atom_value>::value, nullptr))
    return false;
  auto ptr = xs->get(pos);
  if (me.v != *reinterpret_cast<const atom_value*>(ptr))
    return false;
  // This assignment casts `atom_value` to `atom_constant<V>*`.
  // This type violation could theoretically cause undefined behavior.
  // However, `uti` does have an address that is guaranteed to be valid
  // throughout the runtime of the program and the atom constant
  // does not have any members. Hence, this is nonetheless safe since
  // we are never actually going to dereference the pointer.
  *storage = const_cast<void*>(ptr);
  return true;
}

bool try_match(const type_erased_tuple* xs,
               pattern_iterator iter, size_t ps, void** out) {
  CAF_ASSERT(out != nullptr);
  if (! xs)
    return ps == 0;
  if (xs->size() != ps)
    return false;
  for (size_t i = 0; i < ps; ++i, ++iter, ++out)
    // inspect current element
    if (! iter->fun(*iter, xs, i, out))
      // type mismatch
      return false;
  return true;
}

} // namespace detail
} // namespace caf
