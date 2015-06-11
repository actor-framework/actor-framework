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

#ifndef CAF_DETAIL_SPAWN_FWD_HPP
#define CAF_DETAIL_SPAWN_FWD_HPP

#include <functional>

#include "caf/fwd.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

/// Converts `scoped_actor` and pointers to actors to handles of type `actor`
/// but simply forwards any other argument in the same way `std::forward` does.
template <class T>
typename std::conditional<
  is_convertible_to_actor<typename std::decay<T>::type>::value,
  actor,
  T&&
>::type
spawn_fwd(typename std::remove_reference<T>::type& arg) noexcept {
  return static_cast<T&&>(arg);
}

/// Converts `scoped_actor` and pointers to actors to handles of type `actor`
/// but simply forwards any other argument in the same way `std::forward` does.
template <class T>
typename std::conditional<
  is_convertible_to_actor<typename std::decay<T>::type>::value,
  actor,
  T&&
>::type
spawn_fwd(typename std::remove_reference<T>::type&& arg) noexcept {
  static_assert(! std::is_lvalue_reference<T>::value,
                "silently converting an lvalue to an rvalue");
  return static_cast<T&&>(arg);
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_SPAWN_FWD_HPP
