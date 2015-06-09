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

#ifndef CAF_TYPE_NAME_ACCESS_HPP
#define CAF_TYPE_NAME_ACCESS_HPP

#include <string>

#include "caf/uniform_typeid.hpp"
#include "caf/uniform_type_info.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {

template <class T>
std::string type_name_access_impl(std::true_type) {
  return T::static_type_name();
}

template <class T>
std::string type_name_access_impl(std::false_type) {
  auto uti = uniform_typeid<T>(true);
  return uti ? uti->name() : "void";
}

template <class T>
std::string type_name_access() {
  std::integral_constant<bool, detail::has_static_type_name<T>::value> token;
  return type_name_access_impl<T>(token);
}

} // namespace caf

#endif // CAF_TYPE_NAME_ACCESS_HPP
