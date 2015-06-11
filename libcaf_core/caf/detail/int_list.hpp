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

#ifndef CAF_DETAIL_INT_LIST_HPP
#define CAF_DETAIL_INT_LIST_HPP

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

/// A list of integers (wraps a long... template parameter pack).
template <long... Is>
struct int_list {};

template <size_t N, size_t Size, long... Is>
struct il_right_impl;

template <size_t N, size_t Size>
struct il_right_impl<N, Size> {
  using type = int_list<>;
};

template <size_t N, size_t Size, long I, long... Is>
struct il_right_impl<N, Size, I, Is...> : il_right_impl<N, Size - 1, Is...> { };

template <size_t N, long I, long... Is>
struct il_right_impl<N, N, I, Is...> {
  using type = int_list<I, Is...>;
};

template <class List, size_t N>
struct il_right;

template <long... Is, size_t N>
struct il_right<int_list<Is...>, N> {
  using type = typename il_right_impl<(N > sizeof...(Is) ? sizeof...(Is) : N),
                                      sizeof...(Is), Is...>::type;
};

template <bool Done, size_t Num, class List, long... Is>
struct il_take_impl;

template <class List, long... Is>
struct il_take_impl<true, 0, List, Is...> {
  using type = List;
};

template <size_t Num, long... Rs, long I, long... Is>
struct il_take_impl<false, Num, int_list<Rs...>, I, Is...> {
  using type = typename il_take_impl<Num == 1, Num - 1, int_list<Rs..., I>, Is...>::type;
};

template <class List, size_t N>
struct il_take;

template <long... Is, size_t N>
struct il_take<int_list<Is...>, N> {
  using type = typename il_take_impl<N == 0, N, int_list<>, Is...>::type;
};


/// Creates indices for `List` beginning at `Pos`.
template <class List, long Pos = 0, typename Indices = int_list<>>
struct il_indices;

template <template <class...> class List, long... Is, long Pos>
struct il_indices<List<>, Pos, int_list<Is...>> {
  using type = int_list<Is...>;
};

template <template <class...> class List,
     typename T0,
     class... Ts,
     long Pos,
     long... Is>
struct il_indices<List<T0, Ts...>, Pos, int_list<Is...>> {
  // always use type_list to forward remaining Ts... arguments
  using type =
    typename il_indices<
      type_list<Ts...>,
      Pos + 1,
      int_list<Is..., Pos>
    >::type;
};

template <class T>
typename il_indices<T>::type get_indices(const T&) {
  return {};
}

template <size_t Num, class T>
typename il_right<typename il_indices<T>::type, Num>::type
get_right_indices(const T&) {
  return {};
}

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INT_LIST_HPP
