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

#ifndef CAF_MAKE_COUNTED_HPP
#define CAF_MAKE_COUNTED_HPP

#include "caf/ref_counted.hpp"
#include "caf/intrusive_ptr.hpp"

#include "caf/detail/memory.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {

/// Constructs an object of type `T` in an `intrusive_ptr`.
/// @relates ref_counted
/// @relates intrusive_ptr
template <class T, class... Ts>
typename std::enable_if<
  detail::is_memory_cached<T>::value,
  intrusive_ptr<T>
>::type
make_counted(Ts&&... xs) {
  return intrusive_ptr<T>(detail::memory::create<T>(std::forward<Ts>(xs)...),
                          false);
}

/// Constructs an object of type `T` in an `intrusive_ptr`.
/// @relates ref_counted
/// @relates intrusive_ptr
template <class T, class... Ts>
typename std::enable_if<
  ! detail::is_memory_cached<T>::value,
  intrusive_ptr<T>
>::type
make_counted(Ts&&... xs) {
  return intrusive_ptr<T>(new T(std::forward<Ts>(xs)...), false);
}

} // namespace caf

#endif // CAF_MAKE_COUNTED_HPP
