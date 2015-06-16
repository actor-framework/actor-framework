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

#ifndef CAF_ANNOUNCE_HPP
#define CAF_ANNOUNCE_HPP

#include <memory>
#include <typeinfo>

#include "caf/string_algorithms.hpp"

#include "caf/config.hpp"
#include "caf/uniform_type_info.hpp"
#include "caf/abstract_uniform_type_info.hpp"

#include "caf/detail/safe_equal.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/default_uniform_type_info.hpp"

namespace caf {

/// @addtogroup TypeSystem
/// @{

/// A simple example for announce with public accessible members.
/// The output of this example program is:
/// > foo(1, 2)<br>
/// > foo_pair(3, 4)
/// @example announce_1.cpp

/// An example for announce with getter and setter member functions.
/// The output of this example program is:
///
/// > foo(1, 2)
/// @example announce_2.cpp

/// An example for announce with overloaded getter and setter member functions.
/// The output of this example program is:
///
/// > foo(1, 2)
/// @example announce_3.cpp

/// An example for announce with non-primitive members.
/// The output of this example program is:
///
/// > bar(foo(1, 2), 3)
/// @example announce_4.cpp

/// An advanced example for announce implementing serialization
/// for a user-defined tree data type.
/// @example announce_5.cpp

/// Adds a new mapping to the type system. Returns `utype.get()` on
/// success, otherwise a pointer to the previously installed singleton.
/// @warning `announce` is **not** thead-safe!
const uniform_type_info* announce(const std::type_info& tinfo,
                                  uniform_type_info_ptr utype);

// deals with member pointer
/// Creates meta information for a non-trivial `Member`,
/// whereas `xs` are the "sub-members" of `Member`.
/// @see {@link announce_4.cpp announce example 4}
template <class Member, class Parent, class... Ts>
std::pair<Member Parent::*, abstract_uniform_type_info<Member>*>
compound_member(Member Parent::*memptr, const Ts&... xs) {
  return {memptr, new detail::default_uniform_type_info<Member>("???", xs...)};
}

// deals with getter returning a mutable reference
/// Creates meta information for a non-trivial `Member` accessed
/// via the getter member function `getter` returning a mutable reference,
/// whereas `xs` are the "sub-members" of `Member`.
/// @see {@link announce_4.cpp announce example 4}
template <class Member, class Parent, class... Ts>
std::pair<Member& (Parent::*)(), abstract_uniform_type_info<Member>*>
compound_member(Member& (Parent::*getter)(), const Ts&... xs) {
  return {getter, new detail::default_uniform_type_info<Member>("???", xs...)};
}

// deals with getter/setter pair
/// Creates meta information for a non-trivial `Member` accessed
/// via the pair of getter and setter member function pointers `gspair`,
/// whereas `xs` are the "sub-members" of `Member`.
/// @see {@link announce_4.cpp announce example 4}
template <class Parent, class GRes, class SRes, class SArg, class... Ts>
std::pair<std::pair<GRes (Parent::*)() const, SRes (Parent::*)(SArg)>,
          abstract_uniform_type_info<typename std::decay<GRes>::type>*>
compound_member(const std::pair<GRes (Parent::*)() const,
                                SRes (Parent::*)(SArg)>& gspair,
                const Ts&... xs) {
  using mtype = typename std::decay<GRes>::type;
  return {gspair, new detail::default_uniform_type_info<mtype>("???", xs...)};
}

/// Adds a new type mapping for `T` to the type system using `tname`
/// as its uniform name and the list of member pointers `xs`.
/// @warning `announce` is **not** thead-safe!
template <class T, class... Ts>
inline const uniform_type_info* announce(std::string tname, const Ts&... xs) {
  static_assert(std::is_pod<T>::value || std::is_empty<T>::value
                || detail::is_comparable<T, T>::value,
                "T is neither a POD, nor empty, nor comparable with '=='");
  auto ptr = new detail::default_uniform_type_info<T>(std::move(tname), xs...);
  return announce(typeid(T), uniform_type_info_ptr{ptr});
}

/// @}

} // namespace caf

#endif // CAF_ANNOUNCE_HPP
