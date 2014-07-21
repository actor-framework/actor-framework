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

#ifndef CAF_FROM_STRING_HPP
#define CAF_FROM_STRING_HPP

#include <string>
#include <typeinfo>
#include <exception>

#include "caf/uniform_type_info.hpp"

namespace caf {

/**
 * @brief Converts a string created by {@link caf::to_string to_string}
 *    to its original value.
 * @param what String representation of a serialized value.
 * @returns An {@link caf::object object} instance that contains
 *      the deserialized value.
 */
uniform_value from_string_impl(const std::string& what);

/**
 * @brief Convenience function that deserializes a value from @p what and
 *    converts the result to @p T.
 * @throws std::logic_error if the result is not of type @p T.
 * @returns The deserialized value as instance of @p T.
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

#endif // CAF_FROM_STRING_HPP
