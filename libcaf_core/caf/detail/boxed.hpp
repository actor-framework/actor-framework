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

#ifndef CAF_DETAIL_BOXED_HPP
#define CAF_DETAIL_BOXED_HPP

#include "caf/anything.hpp"
#include "caf/detail/wrapped.hpp"

namespace caf {
namespace detail {

template <class T>
struct boxed {
  constexpr boxed() {
    // nop
  }
  using type = detail::wrapped<T>;
};

template <class T>
struct boxed<detail::wrapped<T>> {
  constexpr boxed() {
    // nop
  }
  using type = detail::wrapped<T>;
};

template <>
struct boxed<anything> {
  constexpr boxed() {
    // nop
  }
  using type = anything;
};

template <class T>
struct is_boxed {
  static constexpr bool value = false;
};

template <class T>
struct is_boxed<detail::wrapped<T>> {
  static constexpr bool value = true;
};

template <class T>
struct is_boxed<detail::wrapped<T>()> {
  static constexpr bool value = true;
};

template <class T>
struct is_boxed<detail::wrapped<T>(&)()> {
  static constexpr bool value = true;
};

template <class T>
struct is_boxed<detail::wrapped<T>(*)()> {
  static constexpr bool value = true;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_BOXED_HPP
