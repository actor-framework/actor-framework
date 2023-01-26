// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

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
/// For example, `extend<one, T>::with<two, three>` is an alias for
/// `three<two<one, T>, T>`.
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
