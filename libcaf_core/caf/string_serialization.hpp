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

#ifndef CAF_STRING_SERIALIZATION_HPP
#define CAF_STRING_SERIALIZATION_HPP

#include <string>

#include "caf/fwd.hpp"
#include "caf/optional.hpp"
#include "caf/uniform_type_info.hpp"

namespace std {
class exception;
}

namespace caf {

std::string to_string(const message& what);

std::string to_string(const group& what);

std::string to_string(const channel& what);

std::string to_string(const message_id& what);

std::string to_string(const actor_addr& what);

std::string to_string(const actor& what);

/**
 * @relates node_id
 */
//std::string to_string(const node_id::host_id_type& node_id);

/**
 * @relates node_id
 */
std::string to_string(const node_id& what);

/**
 * Returns `what` as a string representation.
 */
std::string to_string(const atom_value& what);

/**
 * Converts `e` to a string including the demangled type of `e` and `e.what()`.
 */
std::string to_verbose_string(const std::exception& e);

/**
 * Converts a string created by `to_string` to its original value.
 */
uniform_value from_string_impl(const std::string& what);

/**
 * Convenience function that tries to deserializes a value from
 * `what` and converts the result to `T`.
 */
template <class T>
optional<T> from_string(const std::string& what) {
  auto uti = uniform_typeid<T>();
  auto uv = from_string_impl(what);
  if (!uv || (*uv->ti) != typeid(T)) {
    // try again using the type name
    std::string tmp = uti->name();
    tmp += " ( ";
    tmp += what;
    tmp += " )";
    uv = from_string_impl(tmp);
  }
  if (uv && (*uv->ti) == typeid(T)) {
    return T{std::move(*reinterpret_cast<T*>(uv->val))};
  }
  return none;
}

} // namespace caf

#endif // CAF_STRING_SERIALIZATION_HPP
