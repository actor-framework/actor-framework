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

#ifndef CAF_DETAIL_UNBOXED_HPP
#define CAF_DETAIL_UNBOXED_HPP

#include <memory>

#include "caf/atom.hpp"
#include "caf/detail/wrapped.hpp"

namespace caf {
namespace detail {

// strips `wrapped` and converts `atom_constant` to `atom_value`
template <class T>
struct unboxed {
  using type = T;
};

template <class T>
struct unboxed<detail::wrapped<T>> {
  using type = typename detail::wrapped<T>::type;
};

template <class T>
struct unboxed<detail::wrapped<T>(&)()> {
  using type = typename detail::wrapped<T>::type;
};

template <class T>
struct unboxed<detail::wrapped<T>()> {
  using type = typename detail::wrapped<T>::type;
};

template <class T>
struct unboxed<detail::wrapped<T>(*)()> {
  using type = typename detail::wrapped<T>::type;
};

template <atom_value V>
struct unboxed<atom_constant<V>> {
  using type = atom_value;
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_UNBOXED_HPP
