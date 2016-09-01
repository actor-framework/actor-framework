/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
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

#ifndef CAF_MAKE_TYPE_ERASED_VALUE_HPP
#define CAF_MAKE_TYPE_ERASED_VALUE_HPP

#include <cstdint>
#include <typeinfo>
#include <functional>

#include "caf/type_erased_value.hpp"

#include "caf/detail/type_erased_value_impl.hpp"

namespace caf {

/// @relates type_erased_value
/// Creates a type-erased value of type `T` from `xs`.
template <class T, class... Ts>
type_erased_value_ptr make_type_erased_value(Ts&&... xs) {
  type_erased_value_ptr result;
  result.reset(new detail::type_erased_value_impl<T>(std::forward<Ts>(xs)...));
  return result;
}

/// @relates type_erased_value
/// Converts values to type-erased values.
struct type_erased_value_factory {
  template <class T>
  type_erased_value_ptr operator()(T&& x) const {
    return make_type_erased_value<typename std::decay<T>::type>(std::forward<T>(x));
  }
};

} // namespace caf

#endif // CAF_MAKE_TYPE_ERASED_VALUE_HPP
