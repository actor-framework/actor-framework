// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/type_list.hpp"

#include <cstddef>

namespace caf::detail {

/// A list of integers (wraps a long... template parameter pack).
template <long... Is>
struct int_list {};

template <size_t N, size_t Size, long... Is>
struct il_right_impl;

template <size_t N, size_t Size>
struct il_right_impl<N, Size> {
  using type = int_list<>;
};

/// Convenience alias for `il_right_impl<N, Size, Is...>::type`.
template <size_t N, size_t Size, long... Is>
using il_right_impl_t = typename il_right_impl<N, Size, Is...>::type;

template <size_t N, size_t Size, long I, long... Is>
struct il_right_impl<N, Size, I, Is...> : il_right_impl<N, Size - 1, Is...> {};

template <size_t N, long I, long... Is>
struct il_right_impl<N, N, I, Is...> {
  using type = int_list<I, Is...>;
};

template <class List, size_t N>
struct il_right;

template <long... Is, size_t N>
struct il_right<int_list<Is...>, N> {
  using type = il_right_impl_t<(N > sizeof...(Is) ? sizeof...(Is) : N),
                               sizeof...(Is), Is...>;
};

/// Convenience alias for `il_right<List, N>::type`.
template <class List, size_t N>
using il_right_t = typename il_right<List, N>::type;

template <bool Done, size_t Num, class List, long... Is>
struct il_take_impl;

template <class List, long... Is>
struct il_take_impl<true, 0, List, Is...> {
  using type = List;
};

template <size_t Num, long... Rs, long I, long... Is>
struct il_take_impl<false, Num, int_list<Rs...>, I, Is...> {
  using type =
    typename il_take_impl<Num == 1, Num - 1, int_list<Rs..., I>, Is...>::type;
};

/// Convenience alias for `il_take_impl<Done, Num, List, Is...>::type`.
template <bool Done, size_t Num, class List, long... Is>
using il_take_impl_t = typename il_take_impl<Done, Num, List, Is...>::type;

template <class List, size_t N>
struct il_take;

template <long... Is, size_t N>
struct il_take<int_list<Is...>, N> {
  using type = il_take_impl_t<N == 0, N, int_list<>, Is...>;
};

/// Creates indices for `List` beginning at `Pos`.
template <class List, long Pos = 0, typename Indices = int_list<>>
struct il_indices;

template <template <class...> class List, long... Is, long Pos>
struct il_indices<List<>, Pos, int_list<Is...>> {
  using type = int_list<Is...>;
};

template <template <class...> class List, typename T0, class... Ts, long Pos,
          long... Is>
struct il_indices<List<T0, Ts...>, Pos, int_list<Is...>> {
  // always use type_list to forward remaining Ts... arguments
  using type =
    typename il_indices<type_list<Ts...>, Pos + 1, int_list<Is..., Pos>>::type;
};

/// Convenience alias for `il_indices<T>::type`.
template <class T>
using il_indices_t = typename il_indices<T>::type;

/// @cond
template <class T>
il_indices_t<T> get_indices(const T&) {
  return {};
}

template <size_t Num, class T>
il_right_t<il_indices_t<T>, Num> get_right_indices(const T&) {
  return {};
}

template <long First, long Last, long... Is>
struct il_range : il_range<First + 1, Last, Is..., First> {};

template <long Last, long... Is>
struct il_range<Last, Last, Is...> {
  using type = int_list<Is...>;
};
/// @endcond

/// Convenience alias for `il_range<long First, long Last, long... Is>::type`.
template <long First, long Last, long... Is>
using il_range_t = typename il_range<First, Last>::type;

} // namespace caf::detail
