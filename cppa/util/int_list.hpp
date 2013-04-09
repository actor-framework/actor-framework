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
 * Copyright (C) 2011-2013                                                    *
 * Dominik Charousset <dominik.charousset@haw-hamburg.de>                     *
 *                                                                            *
 * This file is part of libcppa.                                              *
 * libcppa is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU Lesser General Public License as published by the     *
 * Free Software Foundation; either version 2.1 of the License,               *
 * or (at your option) any later version.                                     *
 *                                                                            *
 * libcppa is distributed in the hope that it will be useful,                 *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                       *
 * See the GNU Lesser General Public License for more details.                *
 *                                                                            *
 * You should have received a copy of the GNU Lesser General Public License   *
 * along with libcppa. If not, see <http://www.gnu.org/licenses/>.            *
\******************************************************************************/


#ifndef CPPA_UTIL_INT_LIST_HPP
#define CPPA_UTIL_INT_LIST_HPP

#include "cppa/util/type_list.hpp"

namespace cppa { namespace util {

/**
 * @defgroup MetaProgramming Metaprogramming utility.
 */

/**
 * @addtogroup MetaProgramming
 * @{
 */

/**
 * @brief A list of integers (wraps a long... template parameter pack).
 */
template<long... Ts>
struct int_list { };

/**
 * @brief Denotes the empty list.
 */
typedef int_list<> empty_int_list;

template<class List>
struct is_int_list {
    static constexpr bool value = false;
};

template<long... Is>
struct is_int_list<int_list<Is...> > {
    static constexpr bool value = true;
};

// long head(int_list)

/**
 * @brief Gets the first element of @p List.
 */
template<class List>
struct il_head;

template<long I0, long... Is>
struct il_head<int_list<I0,Is...>> {
    static constexpr long value = I0;
};


// int_list tail(int_list)

/**
 * @brief Gets the tail of @p List.
 */
template<class List>
struct il_tail;

template<>
struct il_tail<empty_int_list> {
    typedef empty_int_list type;
};

template<long I0, long... Is>
struct il_tail<int_list<I0,Is...>> {
    typedef int_list<Is...> type;
};


// size_t size(int_list)

/**
 * @brief Gets the number of elements of @p List.
 */
template<class List>
struct il_size;

template<long... Is>
struct il_size<int_list<Is...>> {
    static constexpr size_t value = sizeof...(Is);
};

// T back(int_list)

/**
 * @brief Gets the last element in @p List.
 */
template<class List>
struct il_back;

template<long I0>
struct il_back<int_list<I0>> {
    static constexpr long value = I0;
};

template<long I0, long I1, long... Is>
struct il_back<int_list<I0,I1,Is...> > {
    static constexpr long value = il_back<int_list<I1,Is...>>::value;
};


// bool empty(int_list)

/**
 * @brief Tests whether a list is empty.
 */
template<class List>
struct il_empty {
    static constexpr bool value = std::is_same<empty_int_list,List>::value;
};


/**
 * @brief Gets the minimal value in @p List.
 */
template<class List>
struct il_min;

template<long I0>
struct il_min<int_list<I0>> {
    static constexpr long value = I0;
};

template<long I0, long I1, long... Is>
struct il_min<int_list<I0,I1,Is...> > {
    static constexpr long value = il_min<int_list<(I0 < I1) ? I0 : I1,Is...>>::value;
};


/**
 * @brief Gets the maximum value in @p List.
 */
template<class IndicesPack>
struct il_max;

template<long I0>
struct il_max<int_list<I0>> {
    static constexpr long value = I0;
};

template<long I0, long I1, long... Is>
struct il_max<int_list<I0,I1,Is...> > {
    static constexpr long value = il_max<int_list<(I0 > I1) ? I0 : I1,Is...>>::value;
};


// list slice(size_t, size_t)

template<size_t LeftOffset, size_t Remaining,
         long PadValue, class List, long... Is>
struct il_slice_impl {
    typedef typename il_slice_impl<
                LeftOffset - 1,
                Remaining,
                PadValue,
                typename il_tail<List>::type,
                Is...
            >::type
            type;
};

template<size_t Remaining, long PadValue, class List, long... Is>
struct il_slice_impl<0, Remaining, PadValue, List, Is...> {
    typedef typename il_slice_impl<
                0,
                Remaining - 1,
                PadValue,
                typename il_tail<List>::type,
                Is...,
                il_head<List>::value
            >::type
            type;
};

template<size_t Remaining, long PadValue, long... Is>
struct il_slice_impl<0, Remaining, PadValue, empty_int_list, Is...> {
    typedef typename il_slice_impl<
                0,
                Remaining - 1,
                PadValue,
                empty_int_list,
                Is...,
                PadValue
            >::type
            type;
};

template<long PadValue, class List, long... Is>
struct il_slice_impl<0, 0, PadValue, List, Is...> {
    typedef int_list<Is...> type;
};

template<long PadValue, long... Is>
struct il_slice_impl<0, 0, PadValue, empty_int_list, Is...> {
    typedef int_list<Is...> type;
};

template<class List, size_t ListSize, size_t First, size_t Last, long PadValue = 0>
struct il_slice_ {
    typedef typename il_slice_impl<First,(Last-First),PadValue,List>::type type;
};

template<class List, size_t ListSize, long PadValue>
struct il_slice_<List,ListSize,0,ListSize,PadValue> {
    typedef List type;
};

/**
 * @brief Creates a new list from range (First, Last].
 */
template<class List, size_t First, size_t Last>
struct il_slice {
    static_assert(First <= Last, "First > Last");
    typedef typename il_slice_<List,il_size<List>::value,First,Last>::type type;
};

/**
 * @brief Creates a new list containing the last @p N elements.
 */
template<class List, size_t N>
struct il_right {
    static constexpr size_t list_size = il_size<List>::value;
    static constexpr size_t first_idx = (list_size > N) ? (list_size - N) : 0;
    typedef typename il_slice<List,first_idx,list_size>::type type;
};

template<size_t N>
struct il_right<empty_int_list,N> {
    typedef empty_int_list type;
};

// list reverse()

template<class List, long... Vs>
struct il_reverse_impl;

template<long I0, long... Is, long... Vs>
struct il_reverse_impl<int_list<I0,Is...>,Vs...> {
    typedef typename il_reverse_impl<int_list<Is...>,I0,Vs...>::type type;
};

template<long... Vs>
struct il_reverse_impl<empty_int_list,Vs...> {
    typedef int_list<Vs...> type;
};

/**
 * @brief Creates a new list wih elements in reversed order.
 */
template<class List>
struct il_reverse {
    typedef typename il_reverse_impl<List>::type type;
};


template<class List1, class List2>
struct il_concat_impl;

template<long... As, long... Bs>
struct il_concat_impl<int_list<As...>,int_list<Bs...>> {
    typedef int_list<As...,Bs...> type;
};

// static list concat(list, list)

/**
 * @brief Concatenates lists.
 */
template<class... Lists>
struct il_concat;

template<class List0>
struct il_concat<List0> {
    typedef List0 type;
};

template<class List0, class List1, class... Lists>
struct il_concat<List0,List1,Lists...> {
    typedef typename il_concat<
                typename il_concat_impl<List0,List1>::type,
                Lists...
            >::type
            type;
};

// list push_back(list, type)

template<class List, long Val>
struct il_push_back;

/**
 * @brief Appends @p What to given list.
 */
template<long... Is, long Val>
struct il_push_back<int_list<Is...>,Val> {
    typedef int_list<Is...,Val> type;
};

template<class List, long Val>
struct il_push_front;

/**
 * @brief Appends @p What to given list.
 */
template<long... Is, long Val>
struct il_push_front<int_list<Is...>,Val> {
    typedef int_list<Val,Is...> type;
};


// list pop_back()

/**
 * @brief Creates a new list wih all but the last element of @p List.
 */
template<class List>
struct il_pop_back {
    typedef typename il_slice<List,0,il_size<List>::value-1>::type type;
};

template<>
struct il_pop_back<empty_int_list> {
    typedef empty_int_list type;
};

// type at(size_t)

template<size_t N, long... Is>
struct il_at_impl;

template<size_t N, long I0, long... Is>
struct il_at_impl<N,I0,Is...> : il_at_impl<N-1,Is...> { };

template<long I0, long... Is>
struct il_at_impl<0,I0,Is...> {
    static constexpr long value = I0;
};

template<class List, size_t N>
struct il_at;

/**
 * @brief Gets element at index @p N of @p List.
 */
template<size_t N, long... E>
struct il_at<int_list<E...>, N> {
    static_assert(N < sizeof...(E), "N >= il_size<List>::value");
    typedef typename il_at_impl<N, E...>::type type;
};

// list prepend(type)

template<class List, long What>
struct il_prepend;

/**
 * @brief Creates a new list with @p What prepended to @p List.
 */
template<long Val, long... Is>
struct il_prepend<int_list<Is...>,Val> {
    typedef int_list<Val,Is...> type;
};

// list resize(list, size, fill_type)

template<class List, bool OldSizeLessNewSize,
         size_t OldSize, size_t NewSize, long FillVal>
struct il_pad_right_impl;

template<class List, size_t OldSize, size_t NewSize, long FillVal>
struct il_pad_right_impl<List,false,OldSize,NewSize,FillVal> {
    typedef typename il_slice<List,0,NewSize>::type type;
};

template<class List, size_t Size, long FillVal>
struct il_pad_right_impl<List,false,Size,Size,FillVal> {
    typedef List type;
};

template<class List, size_t OldSize, size_t NewSize, long FillVal>
struct il_pad_right_impl<List, true, OldSize, NewSize, FillVal> {
    typedef typename il_pad_right_impl<
                typename il_push_back<List, FillVal>::type, (OldSize + 1) < NewSize,
                OldSize + 1,
                NewSize,
                FillVal
            >::type
            type;
};

/**
 * @brief Resizes the list to contain @p NewSize elements and uses
 *        @p FillVal to initialize the new elements with.
 */
template<class List, size_t NewSize, long FillVal = 0>
struct il_pad_right {
    typedef typename il_pad_right_impl<
            List, (il_size<List>::value < NewSize), il_size<List>::value, NewSize, FillVal
            >::type
            type;
};

// bool pad_left(list, N)

template<class List, size_t OldSize, size_t NewSize, long FillVal>
struct il_pad_left_impl {
    typedef typename il_pad_left_impl<
                typename il_push_front<List, FillVal>::type,
                OldSize + 1,
                NewSize,
                FillVal
            >::type
            type;
};

template<class List, size_t Size, long FillVal>
struct il_pad_left_impl<List, Size, Size, FillVal> {
    typedef List type;
};

/**
 * @brief Resizes the list to contain @p NewSize elements and uses
 *        @p FillVal to initialize prepended elements with.
 */
template<class List, size_t NewSize, long FillVal = 0>
struct il_pad_left {
    static constexpr size_t list_size = il_size<List>::value;
    //static_assert(NewSize >= il_size<List>::value, "List too big");
    typedef typename il_pad_left_impl<
                List,
                list_size, (list_size > NewSize) ? list_size : NewSize,
                FillVal
            >::type
            type;
};

/**
 * @brief Creates the list (From, ..., To)
 */
template<long From, long To, long... Is>
struct il_range {
    typedef typename il_range<From,To-1,To,Is...>::type type;
};

template<long X, long... Is>
struct il_range<X,X,Is...> {
    typedef int_list<X,Is...> type;
};

/**
 * @brief Creates indices for @p List beginning at @p Pos.
 */
template<typename List, long Pos = 0, typename Indices = empty_int_list>
struct il_indices;

template<template<class...> class List, long... Is, long Pos>
struct il_indices<List<>,Pos,int_list<Is...>> {
    typedef int_list<Is...> type;
};

template<template<class...> class List, typename T0, typename... Ts, long Pos, long... Is>
struct il_indices<List<T0,Ts...>,Pos,int_list<Is...>> {
    // always use type_list to forward remaining Ts... arguments
    typedef typename il_indices<type_list<Ts...>,Pos+1,int_list<Is...,Pos>>::type type;
};

template<typename T>
constexpr auto get_indices(const T&) -> typename il_indices<T>::type {
    return {};
}

template<size_t Num, typename T>
constexpr auto get_right_indices(const T&)
-> typename il_right<typename il_indices<T>::type,Num>::type {
    return {};
}

/**
 * @}
 */

} } // namespace cppa::util

#endif // CPPA_UTIL_INT_LIST_HPP
