/******************************************************************************\
 *           ___        __                                                    *
 *          /\_ \    __/\ \                                                   *
 *          \//\ \  /\_\ \ \____    ___   _____   _____      __               *
 *            \ \ \ \/\ \ \ '__`\  /'___\/\ '__`\/\ '__`\  /'__`\             *
 *             \_\ \_\ \ \ \ \L\ \/\ \__/\ \ \L\ \ \ \L\ \/\ \L\.\_           *
 *             /\____\\ \_\ \_,__/\ \____\\ \ ,__/\ \ ,__/\ \__/.\_\          *
 *             \/____/ \/_/\/___/  \/____/ \ \ \/  \ \ \/  \/__/\/_/          *
 *                                          \ \_\   \ \_\                     *
 *                                           \/_/    \/_/                     *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the Boost Software License, Version 1.0. See             *
 * accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt  *
\******************************************************************************/

#ifndef CAF_DETAIL_INT_LIST_HPP
#define CAF_DETAIL_INT_LIST_HPP

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

/**
 * @addtogroup MetaProgramming
 * @{
 */

/**
 * @brief A list of integers (wraps a long... template parameter pack).
 */
template<long... Is>
struct int_list {};

template<size_t N, size_t Size, long... Is>
struct il_right_impl;

template<size_t N, size_t Size>
struct il_right_impl<N, Size> {
    using type = int_list<>;
};

template<size_t N, size_t Size, long I, long... Is>
struct il_right_impl<N, Size, I, Is...> : il_right_impl<N, Size - 1, Is...> { };

template<size_t N, long I, long... Is>
struct il_right_impl<N, N, I, Is...> {
    using type = int_list<I, Is...>;
};

template<class List, size_t N>
struct il_right;

template<long... Is, size_t N>
struct il_right<int_list<Is...>, N> : il_right_impl<(N > sizeof...(Is)
                                                       ? sizeof...(Is)
                                                       : N),
                                                    sizeof...(Is),
                                                    Is...> { };

/**
 * @brief Creates indices for @p List beginning at @p Pos.
 */
template<typename List, long Pos = 0, typename Indices = int_list<>>
struct il_indices;

template<template<class...> class List, long... Is, long Pos>
struct il_indices<List<>, Pos, int_list<Is...>> {
    using type = int_list<Is...>;
};

template<template<class...> class List,
         typename T0,
         typename... Ts,
         long Pos,
         long... Is>
struct il_indices<List<T0, Ts...>, Pos, int_list<Is...>> {
    // always use type_list to forward remaining Ts... arguments
    using type = typename il_indices<
                     type_list<Ts...>,
                     Pos + 1,
                     int_list<Is..., Pos>
                 >::type;

};

template<typename T>
constexpr auto get_indices(const T&) -> typename il_indices<T>::type {
    return {};
}

template<size_t Num, typename T>
constexpr auto get_right_indices(const T&)
-> typename il_right<typename il_indices<T>::type, Num>::type {
    return {};
}

/**
 * @}
 */

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_INT_LIST_HPP
