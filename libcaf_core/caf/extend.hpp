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

#ifndef CAF_MIXED_HPP
#define CAF_MIXED_HPP

namespace caf {

namespace detail {

template <class D, class B, template <class, class> class... Ms>
struct extend_helper;

template <class D, class B>
struct extend_helper<D, B> {
  using type = B;
};

template <class D, class B, template <class, class> class M,
      template <class, class> class... Ms>
struct extend_helper<D, B, M, Ms...> : extend_helper<D, M<B, D>, Ms...> {
  // no content
};

} // namespace detail

/// Allows convenient definition of types using mixins.
/// For example, `extend<ar, T>::with<ob, fo>` is an alias for
/// `fo<ob<ar, T>, T>`.
///
/// Mixins always have two template parameters: base type and
/// derived type. This allows mixins to make use of the curiously recurring
/// template pattern (CRTP). However, if none of the used mixins use CRTP,
/// the second template argument can be ignored (it is then set to Base).
template <class Base, class Derived = Base>
struct extend {
  /// Identifies the combined type.
  template <template <class, class> class... Mixins>
  using with = typename detail::extend_helper<Derived, Base, Mixins...>::type;
};

} // namespace caf

#endif // CAF_MIXED_HPP
