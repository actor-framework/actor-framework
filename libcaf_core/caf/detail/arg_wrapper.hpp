/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2017                                                  *
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

#ifndef CAF_ARG_WRAPPER_HPP
#define CAF_ARG_WRAPPER_HPP

#include <tuple>
#include <string>

#include "caf/detail/int_list.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/detail/apply_args.hpp"
#include "caf/detail/type_traits.hpp"

namespace caf {
namespace detail {

/// Enables automagical string conversion for `CAF_ARG`.
template <class T>
struct arg_wrapper {
  const char* name;
  const T& value;
  arg_wrapper(const char* x, const T& y) : name(x), value(y) {
    // nop
  }
};

template <class... Ts>
struct named_args_tuple {
  const std::array<const char*, sizeof...(Ts)>& names;
  std::tuple<const Ts&...> xs;
};

template <class... Ts>
named_args_tuple<Ts...>
make_named_args_tuple(std::array<const char*, sizeof...(Ts)>& names,
                      const Ts&... xs) {
  return {names, std::forward_as_tuple(xs...)};
};

template <size_t I, class... Ts>
auto get(const named_args_tuple<Ts...>& x)
-> arg_wrapper<typename type_at<I, Ts...>::type>{
  CAF_ASSERT(x.names[I] != nullptr);
  return {x.names[I], std::get<I>(x.xs)};
}

/// Used to implement `CAF_ARG`.
template <class T>
static arg_wrapper<T> make_arg_wrapper(const char* name, const T& value) {
  return {name, value};
}

struct arg_wrapper_maker {
  template <class... Ts>
  auto operator()(const Ts&... xs) const -> decltype(std::make_tuple(xs...)) {
    return std::make_tuple(xs...);
  }
};

/// Used to implement `CAF_ARGS`.
template <class... Ts>
static std::tuple<arg_wrapper<Ts>...>
make_args_wrapper(std::array<const char*, sizeof...(Ts)> names,
                  const Ts&... xs) {
  arg_wrapper_maker f;
  typename il_range<0, sizeof...(Ts)>::type indices;
  auto tup = make_named_args_tuple(names, xs...);
  return apply_args(f, indices, tup);
}

} // namespace detail
} // namespace caf

#endif // CAF_ARG_WRAPPER_HPP
