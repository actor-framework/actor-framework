// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/type_list.hpp"

namespace caf::detail {

template <int Pos, class Xs, class Ys>
struct imi_result {
  static constexpr int value = Pos;
  using xs = Xs;
  using ys = Ys;
};

template <int Pos, class... Ys>
constexpr auto match_interface(type_list<>, type_list<Ys...>) {
  return imi_result<(sizeof...(Ys) == 0 ? Pos : -1), type_list<>,
                    type_list<Ys...>>{};
}

template <class Signature>
struct is_special_handler : std::false_type {};

template <>
struct is_special_handler<result<void>(down_msg)> : std::true_type {};

template <>
struct is_special_handler<result<void>(exit_msg)> : std::true_type {};

template <>
struct is_special_handler<result<void>(error)> : std::true_type {};

template <>
struct is_special_handler<result<void>(node_down_msg)> : std::true_type {};

template <int Pos, class X, class... Xs, class Ys>
constexpr auto match_interface(type_list<X, Xs...>, Ys) {
  if constexpr (is_special_handler<X>::value) {
    return match_interface<Pos + 1>(type_list_v<Xs...>, Ys{});
  } else if constexpr (tl_contains_v<Ys, X>) {
    return match_interface<Pos + 1>(type_list_v<Xs...>, tl_remove_t<Ys, X>{});
  } else if constexpr (is_timeout_definition_v<X> && sizeof...(Xs) == 0) {
    return match_interface<Pos + 1>(type_list_v<Xs...>, Ys{});
  } else {
    return imi_result<Pos, type_list<X, Xs...>, Ys>{};
  }
}

} // namespace caf::detail

namespace caf {

/// Scans two typed MPI lists for compatibility, returning the index of the
/// first mismatch. Returns the number of elements on a match.
/// @pre len(Found) == len(Expected)
template <class Found, class Expected>
using interface_mismatch_t
  = decltype(detail::match_interface<0>(Found{}, Expected{}));

} // namespace caf
