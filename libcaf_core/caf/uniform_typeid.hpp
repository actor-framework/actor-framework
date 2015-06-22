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

#ifndef CAF_UNIFORM_TYPEID_HPP
#define CAF_UNIFORM_TYPEID_HPP

#include <typeinfo>

#include "caf/detail/type_nr.hpp"

namespace caf {

class uniform_type_info;

/// Returns the uniform type info for the builtin type identified by `nr`.
/// @pre `nr > 0 && nr < detail::type_nrs`
const uniform_type_info* uniform_typeid_by_nr(uint16_t nr);

/// Returns the uniform type info for type `tinf`.
/// @param allow_nullptr if set to true, this function returns `nullptr` instead
///                      of throwing an exception on error
const uniform_type_info* uniform_typeid(const std::type_info& tinf,
                                        bool allow_nullptr = false);

template <class T, uint16_t N = detail::type_nr<T>::value>
struct uniform_typeid_getter {
  const uniform_type_info* operator()(bool) const {
    return uniform_typeid_by_nr(N);
  }
};

template <class T>
struct uniform_typeid_getter<T, 0> {
  const uniform_type_info* operator()(bool allow_nullptr) const {
    return uniform_typeid(typeid(T), allow_nullptr);
  }
};

/// Returns the uniform type info for type `T`.
/// @param allow_nullptr if set to true, this function returns `nullptr` instead
///                      of throwing an exception on error
template <class T>
const uniform_type_info* uniform_typeid(bool allow_nullptr = false) {
  uniform_typeid_getter<T> f;
  return f(allow_nullptr);
}

} // namespace caf

#endif // CAF_UNIFORM_TYPEID_HPP
