// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"

#include <tuple>

namespace caf {

/// Defines:
/// - `valid` if typedefs are set, can be used to check if actors accept input
/// - `type` list of output types or `message` for dynamically typed actors
/// - `delegated_type` type above wrapped in a `delegated`
/// - `tuple_type` output types wrapped in a `std::tuple` or `message`
template <class Ts, class... Xs>
struct response_type;

// short-circuit for dynamically typed messaging
template <class... Xs>
struct response_type<none_t, Xs...> {
  static constexpr bool valid = true;
  using type = message;
  using delegated_type = delegated<message>;
  using tuple_type = message;
};

// end of recursion (suppress type definitions for enabling SFINAE)
template <class... Xs>
struct response_type<detail::type_list<>, Xs...> {
  static constexpr bool valid = false;
};

// case #1: no match
template <class Out, class... In, class... Fs, class... Xs>
struct response_type<detail::type_list<Out(In...), Fs...>, Xs...>
  : response_type<detail::type_list<Fs...>, Xs...> {};

// case #2.a: match `result<Out...>(In...)`
template <class... Out, class... In, class... Fs>
struct response_type<detail::type_list<result<Out...>(In...), Fs...>, In...> {
  static constexpr bool valid = true;
  using type = detail::type_list<Out...>;
  using tuple_type = std::tuple<Out...>;
  using delegated_type = delegated<Out...>;
};

// case #2.b: match `Out(In...)`
template <class Out, class... In, class... Fs>
struct response_type<detail::type_list<Out(In...), Fs...>, In...> {
  static constexpr bool valid = true;
  using type = detail::type_list<Out>;
  using tuple_type = std::tuple<Out>;
  using delegated_type = delegated<Out>;
};

/// Computes the response message type for input `In...` from the list of
/// message passing interfaces `Fs`.
template <class Fs, class... In>
using response_type_t = typename response_type<Fs, In...>::type;

/// Computes the response message type for input `In...` from the list of
/// message passing interfaces `Fs` and returns the corresponding
/// `delegated<T>`.
template <class Fs, class... In>
using delegated_response_type_t =
  typename response_type<Fs, In...>::delegated_type;

/// Unboxes `Xs` and calls `response_type`.
template <class Ts, class Xs>
struct response_type_unbox;

template <class Ts, class... Xs>
struct response_type_unbox<Ts, detail::type_list<Xs...>>
  : response_type<Ts, Xs...> {};

template <class Ts>
struct response_type_unbox<Ts, message> : response_type<Ts, message> {};

/// Computes the response message for input `Xs` from the list
/// of message passing interfaces `Ts`.
template <class Ts, class Xs>
using response_type_unbox_t = typename response_type_unbox<Ts, Xs>::type;

} // namespace caf
