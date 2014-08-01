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
 * License 1.0. See accompanying files LICENSE and LICENCE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_TO_STRING_HPP
#define CAF_TO_STRING_HPP

#include <type_traits>

#include "caf/intrusive_ptr.hpp"

#include "caf/atom.hpp" // included for to_string(atom_value)
#include "caf/actor.hpp"
#include "caf/group.hpp"
#include "caf/channel.hpp"
#include "caf/node_id.hpp"
#include "caf/message.hpp"
#include "caf/anything.hpp"
#include "caf/actor_addr.hpp"
#include "caf/abstract_group.hpp"
#include "caf/uniform_type_info.hpp"

namespace std {
class exception;
}

namespace caf {

namespace detail {

std::string to_string_impl(const void* what, const uniform_type_info* utype);

template <class T>
inline std::string to_string_impl(const T& what) {
  return to_string_impl(&what, uniform_typeid<T>());
}

} // namespace detail

inline std::string to_string(const message& what) {
  return detail::to_string_impl(what);
}

inline std::string to_string(const actor& what) {
  return detail::to_string_impl(what);
}

inline std::string to_string(const actor_addr& what) {
  return detail::to_string_impl(what);
}

inline std::string to_string(const group& what) {
  return detail::to_string_impl(what);
}

inline std::string to_string(const channel& what) {
  return detail::to_string_impl(what);
}

inline std::string to_string(const message_id& what) {
  return detail::to_string_impl(what);
}

// implemented in node_id.cpp
std::string to_string(const node_id& what);

// implemented in node_id.cpp
std::string to_string(const node_id& what);

template <class T, class U, class... Us>
std::string to_string(const T& a1, const U& a2, const Us&... as) {
  return to_string(a1) + ", " + to_string(a2, as...);
}

/**
 * Converts `e` to a string including the demangled type of `e` and `e.what()`.
 */
std::string to_verbose_string(const std::exception& e);

} // namespace caf

#endif // CAF_TO_STRING_HPP
