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

#pragma once

#include <cstddef>
#include <type_traits>
#include <utility>

#include "caf/detail/type_list.hpp"

namespace caf {

/// Allows specializing the `sum_type_access` trait for any type that simply
/// wraps a `variant` and exposes it with a `get_data()` member function.
template <class T>
struct default_sum_type_access {
  using types = typename T::types;

  using type0 = typename detail::tl_head<types>::type;

  static constexpr bool specialized = true;

  static constexpr bool is_mutable = true;

  template <int Pos>
  static bool is(const T& x, std::integral_constant<int, Pos> token) {
    return x.get_data().is(token);
  }

  template <int Pos>
  static typename detail::tl_at<types, Pos>::type&
  get(T& x, std::integral_constant<int, Pos> token) {
    return x.get_data().get(token);
  }

  template <int Pos>
  static const typename detail::tl_at<types, Pos>::type&
  get(const T& x, std::integral_constant<int, Pos> token) {
    return x.get_data().get(token);
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
