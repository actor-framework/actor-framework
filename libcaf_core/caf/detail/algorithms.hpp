// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"

#include <vector>

namespace caf::detail {

/// Like `std::for_each`, but for multiple containers and filters elements by
/// predicate.
/// @pre `x.size() <= y.size()` for each `y` in `xs`
template <class F, class Container, class... Containers>
void zip_foreach(F f, Container&& x, Containers&&... xs) {
  for (size_t i = 0; i < x.size(); ++i)
    f(x[i], xs[i]...);
}

/// Like `std::for_each`, but for multiple containers.
/// @pre `x.size() <= y.size()` for each `y` in `xs`
template <class F, class Predicate, class Container, class... Containers>
void zip_foreach_if(F f, Predicate p, Container&& x, Containers&&... xs) {
  for (size_t i = 0; i < x.size(); ++i)
    if (p(x[i], xs[i]...))
      f(x[i], xs[i]...);
}

/// Like `std::accumulate`, but for multiple containers.
/// @pre `x.size() <= y.size()` for each `y` in `xs`
template <class F, class T, class Container, class... Containers>
T zip_fold(F f, T init, Container&& x, Containers&&... xs) {
  for (size_t i = 0; i < x.size(); ++i)
    init = f(init, x[i], xs[i]...);
  return init;
}

/// Like `std::accumulate`, but for multiple containers and filters elements by
/// predicate.
/// @pre `x.size() <= y.size()` for each `y` in `xs`
template <class F, class Predicate, class T, class Container,
          class... Containers>
T zip_fold_if(F f, Predicate p, T init, Container&& x, Containers&&... xs) {
  for (size_t i = 0; i < x.size(); ++i)
    if (p(x[i], xs[i]...))
      init = f(init, x[i], xs[i]...);
  return init;
}

/// Decorates a container of type `T` to appear as container of type `U`.
template <class F, class Container>
struct container_view {
  Container& x;
  using value_type = typename detail::get_callable_trait<F>::result_type;
  size_t size() const noexcept {
    return x.size();
  }
  value_type operator[](size_t i) {
    F f;
    return f(x[i]);
  }
};

/// Returns a container view for `x`.
/// @relates container_view
template <class F, class Container>
container_view<F, Container> make_container_view(Container& x) {
  return {x};
}

/// Like `std::find`, but takes a range instead of an iterator pair and returns
/// a pointer to the found object on success instead of returning an iterator.
template <class T>
typename T::value_type* ptr_find(T& xs, const typename T::value_type& x) {
  for (auto& y : xs)
    if (y == x)
      return &y;
  return nullptr;
}

/// Like `std::find`, but takes a range instead of an iterator pair and returns
/// a pointer to the found object on success instead of returning an iterator.
template <class T>
const typename T::value_type*
ptr_find(const T& xs, const typename T::value_type& x) {
  for (auto& y : xs)
    if (y == x)
      return &y;
  return nullptr;
}

/// Like `std::find_if`, but takes a range instead of an iterator pair and
/// returns a pointer to the found object on success instead of returning an
/// iterator.
template <class T, class Predicate>
typename T::value_type* ptr_find_if(T& xs, Predicate pred) {
  for (auto& x : xs)
    if (pred(x))
      return &x;
  return nullptr;
}

/// Like `std::find_if`, but takes a range instead of an iterator pair and
/// returns a pointer to the found object on success instead of returning an
/// iterator.
template <class T, class Predicate>
const typename T::value_type* ptr_find_if(const T& xs, Predicate pred) {
  for (auto& x : xs)
    if (pred(x))
      return &x;
  return nullptr;
}

} // namespace caf::detail
