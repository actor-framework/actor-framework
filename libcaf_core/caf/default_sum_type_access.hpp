// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "caf/detail/type_list.hpp"
#include "caf/sum_type_token.hpp"

namespace caf {

/// Allows specializing the `sum_type_access` trait for any type that simply
/// wraps a `variant` and exposes it with a `get_data()` member function.
template <class T>
struct default_sum_type_access {
  using types = typename T::types;

  using type0 = typename detail::tl_head<types>::type;

  static constexpr bool specialized = true;

  template <class U, int Pos>
  static bool is(const T& x, sum_type_token<U, Pos> token) {
    return x.get_data().is(pos(token));
  }

  template <class U, int Pos>
  static U& get(T& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U, int Pos>
  static const U& get(const T& x, sum_type_token<U, Pos> token) {
    return x.get_data().get(pos(token));
  }

  template <class U, int Pos>
  static U* get_if(T* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class U, int Pos>
  static const U* get_if(const T* x, sum_type_token<U, Pos> token) {
    return is(*x, token) ? &get(*x, token) : nullptr;
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(T& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }

  template <class Result, class Visitor, class... Ts>
  static Result apply(const T& x, Visitor&& visitor, Ts&&... xs) {
    return x.get_data().template apply<Result>(std::forward<Visitor>(visitor),
                                               std::forward<Ts>(xs)...);
  }
};

} // namespace caf
