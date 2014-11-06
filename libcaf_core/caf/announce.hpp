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

#ifndef CAF_ANNOUNCE_HPP
#define CAF_ANNOUNCE_HPP

#include <memory>
#include <typeinfo>

#include "caf/string_algorithms.hpp"

#include "caf/config.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/abstract_uniform_type_info.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/default_uniform_type_info.hpp"

namespace caf {

/**
 * @addtogroup TypeSystem
 * @{
 */

/**
 * A simple example for announce with public accessible members.
 * The output of this example program is:
 * > foo(1, 2)<br>
 * > foo_pair(3, 4)
 * @example announce_1.cpp
 */

/**
 * An example for announce with getter and setter member functions.
 * The output of this example program is:
 *
 * > foo(1, 2)
 * @example announce_2.cpp
 */

/**
 * An example for announce with overloaded getter and setter member functions.
 * The output of this example program is:
 *
 * > foo(1, 2)
 * @example announce_3.cpp
 */

/**
 * An example for announce with non-primitive members.
 * The output of this example program is:
 *
 * > bar(foo(1, 2), 3)
 * @example announce_4.cpp
 */

/**
 * An advanced example for announce implementing serialization
 * for a user-defined tree data type.
 * @example announce_5.cpp
 */

/**
 * Adds a new mapping to the type system. Returns `false` if a mapping
 * for `tinfo` already exists, otherwise `true`.
 * @warning `announce` is **not** thead-safe!
 */
const uniform_type_info* announce(const std::type_info& tinfo,
                                  uniform_type_info_ptr utype);

// deals with member pointer
/**
 * Creates meta information for a non-trivial member `C`, whereas
 * `args` are the "sub-members" of `c_ptr`.
 * @see {@link announce_4.cpp announce example 4}
 */
template <class C, class Parent, class... Ts>
std::pair<C Parent::*, detail::abstract_uniform_type_info<C>*>
compound_member(C Parent::*c_ptr, const Ts&... args) {
  return {c_ptr, new detail::default_uniform_type_info<C>(args...)};
}

// deals with getter returning a mutable reference
/**
 * Creates meta information for a non-trivial member accessed
 * via a getter returning a mutable reference, whereas
 * `args` are the "sub-members" of `c_ptr`.
 * @see {@link announce_4.cpp announce example 4}
 */
template <class C, class Parent, class... Ts>
std::pair<C& (Parent::*)(), detail::abstract_uniform_type_info<C>*>
compound_member(C& (Parent::*getter)(), const Ts&... args) {
  return {getter, new detail::default_uniform_type_info<C>(args...)};
}

// deals with getter/setter pair
/**
 * Creates meta information for a non-trivial member accessed
 * via a pair of getter and setter member function pointers, whereas
 * `args` are the "sub-members" of `c_ptr`.
 * @see {@link announce_4.cpp announce example 4}
 */
template <class Parent, class GRes, class SRes, class SArg, class... Ts>
std::pair<std::pair<GRes (Parent::*)() const, SRes (Parent::*)(SArg)>,
          detail::abstract_uniform_type_info<
            typename std::decay<GRes>::type>*>
compound_member(const std::pair<GRes (Parent::*)() const,
                SRes (Parent::*)(SArg)>& gspair,
                const Ts&... args) {
  using mtype = typename std::decay<GRes>::type;
  return {gspair, new detail::default_uniform_type_info<mtype>(args...)};
}

/**
 * Adds a new type mapping for `C` to the type system.
 * @warning `announce` is **not** thead-safe!
 */
template <class C, class... Ts>
inline const uniform_type_info* announce(const Ts&... args) {
  auto ptr = new detail::default_uniform_type_info<C>(args...);
  return announce(typeid(C), uniform_type_info_ptr{ptr});
}

/**
 * @}
 */

} // namespace caf

#endif // CAF_ANNOUNCE_HPP
