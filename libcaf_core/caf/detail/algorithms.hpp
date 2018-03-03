/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#ifndef CAF_DETAIL_ALGORITHMS_HPP
#define CAF_DETAIL_ALGORITHMS_HPP

#include <vector>

#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

/// Like `std::for_each`, but for multiple containers.
/// @pre `x.size() <= y.size()` for each `y` in `xs`
template <class F, class Container, class... Containers>
void zip_foreach(F f, Container&& x, Containers&&... xs) {
  for (size_t i = 0; i < x.size(); ++i)
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

/// Decorates a container of type `T` to appear as container of type `U`.
template <class F, class Container>
struct container_view {
  Container& x;
  using value_type = typename detail::get_callable_trait<F>::result_type;
  inline size_t size() const {
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

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_ALGORITHMS_HPP
