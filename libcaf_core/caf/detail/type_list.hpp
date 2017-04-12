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

#ifndef CAF_DETAIL_TYPE_LIST_HPP
#define CAF_DETAIL_TYPE_LIST_HPP

#include <cstddef>
#include <typeinfo>
#include <type_traits>

#include "caf/unit.hpp"
#include "caf/none.hpp"

#include "caf/detail/tbind.hpp"
#include "caf/detail/type_pair.hpp"

namespace caf {
namespace detail {

/// A list of types.
template <class... Ts>
struct type_list {
  constexpr type_list() {
    // nop
  }
};

/// Denotes the empty list.
using empty_type_list = type_list<>;

template <class T>
struct is_type_list {
  static constexpr bool value = false;
};

template <class... Ts>
struct is_type_list<type_list<Ts...>> {
  static constexpr bool value = true;
};

// Uncomment after having switched to C++14
//template <class T>
//inline constexpr bool is_type_list_v = is_type_list<T>::value;

// T head(type_list)

/// Gets the first element of `List`.
template <class List>
struct tl_head;

template <>
struct tl_head<type_list<>> {
  using type = void;
};

template <typename T0, class... Ts>
struct tl_head<type_list<T0, Ts...>> {
  using type = T0;
};

template <class List>
using tl_head_t = typename tl_head<List>::type;

// type_list tail(type_list)

/// Gets the tail of `List`.
template <class List>
struct tl_tail;

template <>
struct tl_tail<type_list<>> {
  using type = type_list<>;
};

template <typename T0, class... Ts>
struct tl_tail<type_list<T0, Ts...>> {
  using type = type_list<Ts...>;
};

template <class List>
using tl_tail_t = typename tl_tail<List>::type;

// size_t size(type_list)

/// Gets the number of template parameters of `List`.
template <class List>
struct tl_size;

template <class... Ts>
struct tl_size<type_list<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

template <class... Ts>
constexpr size_t tl_size<type_list<Ts...>>::value;

// Uncomment after having switched to C++14
//template <class List>
//inline constexpr size_t tl_size_v = tl_size<List>::value;

// T back(type_list)

/// Gets the last element in `List`.
template <class List>
struct tl_back;

template <>
struct tl_back<type_list<>> {
  using type = unit_t;
};

template <typename T0>
struct tl_back<type_list<T0>> {
  using type = T0;
};

template <typename T0, typename T1, class... Ts>
struct tl_back<type_list<T0, T1, Ts...>> {
  // remaining arguments are forwarded as type_list to prevent
  // recursive instantiation of List class
  using type = typename tl_back<type_list<T1, Ts...>>::type;
};

template <class List>
using tl_back_t = typename tl_back<List>::type;

// bool empty(type_list)

/// Tests whether a list is empty.
template <class List>
struct tl_empty {
  static constexpr bool value = std::is_same<empty_type_list, List>::value;
};

// Uncomment after having switched to C++14
//template <class List>
//inline constexpr bool tl_empty_v = tl_empty<List>::value;

// list slice(size_t, size_t)

template <size_t LeftOffset,
     size_t Remaining,
     typename PadType,
     class List,
     class... Ts>
struct tl_slice_impl {
  using type =
    typename tl_slice_impl<
      LeftOffset - 1,
      Remaining,
      PadType,
      tl_tail_t<List>,
      Ts...
    >::type;
};

template <size_t Remaining, typename PadType, class List, class... Ts>
struct tl_slice_impl<0, Remaining, PadType, List, Ts...> {
  using type =
    typename tl_slice_impl<
      0,
      Remaining - 1,
      PadType,
      tl_tail_t<List>,
      Ts...,
      tl_head_t<List>
    >::type;
};

template <size_t Remaining, typename PadType, class... Ts>
struct tl_slice_impl<0, Remaining, PadType, empty_type_list, Ts...> {
  using type =
    typename tl_slice_impl<
      0,
      Remaining - 1,
      PadType,
      empty_type_list,
      Ts...,
      PadType
    >::type;
};

template <class PadType, class List, class... T>
struct tl_slice_impl<0, 0, PadType, List, T...> {
  using type = type_list<T...>;
};

template <class PadType, class... T>
struct tl_slice_impl<0, 0, PadType, empty_type_list, T...> {
  using type = type_list<T...>;
};

template <class List, size_t ListSize, size_t First, size_t Last,
      typename PadType = unit_t>
struct tl_slice_ {
  using type =
    typename tl_slice_impl<
      First,
      (Last - First),
      PadType,
      List
    >::type;
};

template <class List, size_t ListSize, typename PadType>
struct tl_slice_<List, ListSize, 0, ListSize, PadType> {
  using type = List;
};

/// Creates a new list from range (First, Last].
template <class List, size_t First, size_t Last>
struct tl_slice {
  using type =
    typename tl_slice_<
      List,
      tl_size<List>::value,
      (First > Last ? Last : First),
      Last
    >::type;
};

template <class List, size_t First, size_t Last>
using tl_slice_t = typename tl_slice<List, First, Last>::type;

/// Creates a new list containing the last `N` elements.
template <class List, size_t NewSize, size_t OldSize = tl_size<List>::value>
struct tl_right {
  static constexpr size_t first_idx = OldSize > NewSize ? OldSize - NewSize : 0;
  using type = tl_slice_t<List, first_idx, OldSize>;
};

template <class List, size_t N>
struct tl_right<List, N, N> {
  using type = List;
};

template <size_t N>
struct tl_right<empty_type_list, N, 0> {
  using type = empty_type_list;
};

template <>
struct tl_right<empty_type_list, 0, 0> {
  using type = empty_type_list;
};

template <class List, size_t NewSize, size_t OldSize = tl_size<List>::value>
using tl_right_t = typename tl_right<List, NewSize, OldSize>::type;

template <class ListA, class ListB,
          template <class, typename> class Fun = to_type_pair>
struct tl_zip_impl;

template <class... LhsElements, class... RhsElements,
          template <class, typename> class Fun>
struct tl_zip_impl<type_list<LhsElements...>, type_list<RhsElements...>, Fun> {
  static_assert(sizeof...(LhsElements) == sizeof...(RhsElements),
                "Lists have different size");
  using type = type_list<typename Fun<LhsElements, RhsElements>::type...>;
};

/// Zips two lists of equal size.
///
/// Creates a list formed from the two lists `ListA` and `ListB,`
/// e.g., tl_zip<type_list<int, double>, type_list<float, string>>::type
/// is type_list<type_pair<int, float>, type_pair<double, string>>.
template <class ListA, class ListB, template <class, class> class Fun>
struct tl_zip {
  static constexpr size_t sizea = tl_size<ListA>::value;
  static constexpr size_t sizeb = tl_size<ListB>::value;
  static constexpr size_t result_size = (sizea < sizeb) ? sizea : sizeb;
  using type =
    typename tl_zip_impl<
      tl_slice_t<ListA, 0, result_size>,
      tl_slice_t<ListB, 0, result_size>,
      Fun
    >::type;
};

template <class ListA, class ListB, template <class, class> class Fun>
using tl_zip_t = typename tl_zip<ListA, ListB, Fun>::type;

/// Equal to `zip(right(ListA, N), right(ListB, N), Fun)`.
template <class ListA, class ListB, template <class, class> class Fun, size_t N>
struct tl_zip_right {
  using type =
    typename tl_zip_impl<
      tl_right_t<ListA, N>,
      tl_right_t<ListB, N>,
      Fun
    >::type;
};

template <class ListA, class ListB, template <class, class> class Fun, size_t N>
using tl_zip_right_t = typename tl_zip_right<ListA, ListB, Fun, N>::type;

template <class ListA, class ListB,
          typename PadA = unit_t, typename PadB = unit_t,
          template <class, typename> class Fun = to_type_pair>
struct tl_zip_all {
  static constexpr size_t result_size =
    (tl_size<ListA>::value > tl_size<ListB>::value) ? tl_size<ListA>::value
                                                    : tl_size<ListB>::value;
  using type =
    typename tl_zip_impl<
      typename tl_slice_<ListA, tl_size<ListA>::value, 0, result_size>::type,
      typename tl_slice_<ListB, tl_size<ListB>::value, 0, result_size>::type,
      Fun
    >::type;
};

template <class ListA, class ListB,
          typename PadA = unit_t, typename PadB = unit_t,
          template <class, typename> class Fun = to_type_pair>
using tl_zip_all_t = typename tl_zip_all<ListA, ListB, PadA, PadB, Fun>::type;

template <class ListA>
struct tl_unzip;

template <class... Elements>
struct tl_unzip<type_list<Elements...>> {
  using first = type_list<typename Elements::first...>;
  using second = type_list<typename Elements::second...>;
};

// int index_of(list, type)

/// Finds the first element of type `What` beginning at index `Pos`.
template <int Pos, class X, class... Ts>
struct tl_index_of_impl;

template <int Pos, class X>
struct tl_index_of_impl<Pos, X> {
  static constexpr int value = -1;
};

template <int Pos, class X, class... Ts>
struct tl_index_of_impl<Pos, X, X, Ts...> {
  static constexpr int value = Pos;
};

template <int Pos, class X, class T, class... Ts>
struct tl_index_of_impl<Pos, X, T, Ts...> {
  static constexpr int value = tl_index_of_impl<Pos + 1, X, Ts...>::value;
};

/// Finds the first element satisfying `Pred` beginning at index `Pos`.
template <class List, class T>
struct tl_index_of;

template <class... Ts, class T>
struct tl_index_of<type_list<Ts...>, T> {
  static constexpr int value = tl_index_of_impl<0, T, Ts...>::value;
};

// Uncomment after having switched to C++14
//template <class List, class T>
//inline constexpr int tl_index_of_v = tl_index_of<List, T>::value;

// int index_of(list, predicate)

template <int Pos, template <class> class Pred, class... Ts>
struct tl_index_where_impl;

template <int Pos, template <class> class Pred>
struct tl_index_where_impl<Pos, Pred> {
  static constexpr int value = -1;
};

template <int Pos, template <class> class Pred, class T, class... Ts>
struct tl_index_where_impl<Pos, Pred, T, Ts...> {
  static constexpr int value
    = Pred<T>::value ? Pos : tl_index_where_impl<Pos + 1, Pred, Ts...>::value;
};

/// Finds the first element satisfying a given predicate.
template <class List, template <class> class Pred>
struct tl_index_where;

template <class... Ts, template <class> class Pred>
struct tl_index_where<type_list<Ts...>, Pred> {
  static constexpr int value = tl_index_where_impl<0, Pred, Ts...>::value;
};

// Uncomment after having switched to C++14
//template <class List, template <class> class Pred>
//inline constexpr int tl_index_where_v = tl_index_where<List, Pred>::value;

// list reverse()

template <class From, class... Elements>
struct tl_reverse_impl;

template <class T0, class... T, class... E>
struct tl_reverse_impl<type_list<T0, T...>, E...> {
  using type = typename tl_reverse_impl<type_list<T...>, T0, E...>::type;
};

template <class... E>
struct tl_reverse_impl<empty_type_list, E...> {
  using type = type_list<E...>;
};

/// Creates a new list wih elements in reversed order.
template <class List>
struct tl_reverse {
  using type = typename tl_reverse_impl<List>::type;
};

template <class List>
using tl_reverse_t = typename tl_reverse<List>::type;

// type find(list, predicate)

/// Finds the first element of type `What` beginning at index `Pos`.
template <template <class> class Pred, class... Ts>
struct tl_find_impl;

template <template <class> class Pred>
struct tl_find_impl<Pred> {
  using type = none_t;
};

template <template <class> class Pred, class T, class... Ts>
struct tl_find_impl<Pred, T, Ts...> {
  using type =
    typename std::conditional<
      Pred<T>::value,
      T,
      typename tl_find_impl<Pred, Ts...>::type
    >::type;
};

/// Finds the first element satisfying `Pred` beginning at index `Pos`.
template <class List, template <class> class Pred>
struct tl_find;

template <class... Ts, template <class> class Pred>
struct tl_find<type_list<Ts...>, Pred> {
  using type = typename tl_find_impl<Pred, Ts...>::type;
};

template <class List, template <class> class Pred>
using tl_find_t = typename tl_find<List, Pred>::type;

// bool forall(predicate)

/// Tests whether a predicate holds for all elements of a list.
template <class List, template <class> class Pred>
struct tl_forall {
  static constexpr bool value =
      Pred<tl_head_t<List>>::value
      && tl_forall<tl_tail_t<List>, Pred>::value;
};

template <template <class> class Pred>
struct tl_forall<empty_type_list, Pred> {
  static constexpr bool value = true;
};

// Uncomment after having switched to C++14
//template <class List, template <class> class Pred>
//inline constexpr bool tl_forall_v = tl_forall<List, Pred>::value;

template <class ListA, class ListB, template <class, class> class Pred>
struct tl_forall2_impl {
  static constexpr bool value =
      Pred<tl_head_t<ListA>, tl_head_t<ListB>>::value
      && tl_forall2_impl<tl_tail_t<ListA>, tl_tail_t<ListB>, Pred>::value;
};

template <template <class, typename> class Pred>
struct tl_forall2_impl<empty_type_list, empty_type_list, Pred> {
  static constexpr bool value = true;
};

/// Tests whether a binary predicate holds for all
///    corresponding elements of `ListA` and `ListB`.
template <class ListA, class ListB, template <class, class> class Pred>
struct tl_binary_forall {
  static constexpr bool value =
    tl_size<ListA>::value == tl_size<ListB>::value
    && tl_forall2_impl<ListA, ListB, Pred>::value;
};

// Uncomment after having switched to C++14
//template <class ListA, class ListB, template <class, class> class Pred>
//inline constexpr bool tl_binary_forall_v
//  = tl_binary_forall<ListA, ListB, Pred>::value;

/// Tests whether a predicate holds for some of the elements of a list.
template <class List, template <class> class Pred>
struct tl_exists {
  static constexpr bool value =
    Pred<tl_head_t<List>>::value || tl_exists<tl_tail_t<List>, Pred>::value;
};

template <template <class> class Pred>
struct tl_exists<empty_type_list, Pred> {
  static constexpr bool value = false;
};

// Uncomment after having switched to C++14
//template <class List, template <class> class Pred>
//inline constexpr bool tl_exists_v = tl_exists<List, Pred>::value;

// size_t count(predicate)

/// Counts the number of elements in the list which satisfy a predicate.
template <class List, template <class> class Pred>
struct tl_count {
  static constexpr size_t value =
    (Pred<tl_head_t<List>>::value ? 1 : 0)
    + tl_count<tl_tail_t<List>, Pred>::value;
};

template <template <class> class Pred>
struct tl_count<empty_type_list, Pred> {
  static constexpr size_t value = 0;
};

template <class List, template <class> class Pred>
constexpr size_t tl_count<List, Pred>::value;

// Uncomment after having switched to C++14
//template <class List, template <class> class Pred>
//inline constexpr size_t tl_count_v = tl_count<List, Pred>::value;

// size_t count(type)

/// Counts the number of elements in the list which are equal to `T`.
template <class List, class T>
struct tl_count_type {
  static constexpr size_t value =
    (std::is_same<tl_head_t<List>, T>::value ? 1 : 0)
    + tl_count_type<tl_tail_t<List>, T>::value;
};

template <class T>
struct tl_count_type<empty_type_list, T> {
  static constexpr size_t value = 0;
};

template <class List, class T>
constexpr size_t tl_count_type<List, T>::value;

// Uncomment after having switched to C++14
//template <class List, class T>
//inline constexpr size_t tl_count_type_v = tl_count_type<List, T>::value;

// size_t count_not(predicate)

/// Counts the number of elements in the list which satisfy a predicate.
template <class List, template <class> class Pred>
struct tl_count_not {
  static constexpr size_t value =
    (Pred<tl_head_t<List>>::value ? 0 : 1) +
    tl_count_not<tl_tail_t<List>, Pred>::value;

};

template <template <class> class Pred>
struct tl_count_not<empty_type_list, Pred> {
  static constexpr size_t value = 0;
};

// Uncomment after having switched to C++14
//template <class List, template <class> class Pred>
//inline constexpr size_t tl_count_not_v = tl_count_not<List, Pred>::value;

template <class ListA, class ListB>
struct tl_concat_impl;

/// Concatenates two lists.
template <class... LhsTs, class... RhsTs>
struct tl_concat_impl<type_list<LhsTs...>, type_list<RhsTs...>> {
  using type = type_list<LhsTs..., RhsTs...>;
};

// static list concat(list, list)

/// Concatenates lists.
template <class... Lists>
struct tl_concat;

template <class List0>
struct tl_concat<List0> {
  using type = List0;
};

template <class List0, class List1, class... Lists>
struct tl_concat<List0, List1, Lists...> {
  using type =
    typename tl_concat<
      typename tl_concat_impl<List0, List1>::type,
      Lists...
    >::type;
};

template <class... Lists>
using tl_concat_t = typename tl_concat<Lists...>::type;

// list push_back(list, type)

template <class List, class What>
struct tl_push_back;

/// Appends `What` to given list.
template <class... ListTs, class What>
struct tl_push_back<type_list<ListTs...>, What> {
  using type = type_list<ListTs..., What>;
};

template <class List, class What>
using tl_push_back_t = typename tl_push_back<List, What>::type;

template <class List, class What>
struct tl_push_front;

/// Appends `What` to given list.
template <class... ListTs, class What>
struct tl_push_front<type_list<ListTs...>, What> {
  using type = type_list<What, ListTs...>;
};

template <class List, class What>
using tl_push_front_t = typename tl_push_front<List, What>::type;

/// Alias for `tl_push_front<List, What>`.
template <class What, class List>
struct tl_cons : tl_push_front<List, What> {
  // nop
};

template <class List, class What>
using tl_cons_t = tl_push_front_t<List, What>;

// list map(list, trait)

template <class T, template <class> class... Funs>
struct tl_apply_all;

template <class T>
struct tl_apply_all<T> {
  using type = T;
};

template <class T, template <class> class Fun0, template <class> class... Funs>
struct tl_apply_all<T, Fun0, Funs...> {
  using type = typename tl_apply_all<typename Fun0<T>::type, Funs...>::type;
};

template <class T, template <class> class... Funs>
using tl_apply_all_t = typename tl_apply_all<T, Funs...>::type;

/// Creates a new list by applying a "template function" to each element.
template <class List, template <class> class... Funs>
struct tl_map;

template <class... Ts, template <class> class... Funs>
struct tl_map<type_list<Ts...>, Funs...> {
  using type = type_list<tl_apply_all_t<Ts, Funs...>...>;
};

template <class List, template <class> class... Funs>
using tl_map_t = typename tl_map<List, Funs...>::type;

/// Creates a new list by applying a `Fun` to each element which
///    returns `TraitResult` for `Trait`.
template <class List, template <class> class Trait, bool TRes,
          template <class> class... Funs>
struct tl_map_conditional {
  using type =
    tl_concat_t<
      type_list<
        typename std::conditional<
          Trait<tl_head_t<List>>::value == TRes,
          tl_apply_all_t<tl_head_t<List>, Funs...>,
          tl_head_t<List>
        >::type
      >,
      typename tl_map_conditional<tl_tail_t<List>, Trait, TRes, Funs...>::type
    >;
};

template <template <class> class Trait, bool TraitResult,
         template <class> class... Funs>
struct tl_map_conditional<empty_type_list, Trait, TraitResult, Funs...> {
  using type = empty_type_list;
};

// list pop_back()

/// Creates a new list wih all but the last element of `List`.
template <class List>
struct tl_pop_back {
  using type = typename tl_slice<List, 0, tl_size<List>::value - 1>::type;
};

template <>
struct tl_pop_back<empty_type_list> {
  using type = empty_type_list;
};

template <class List>
using tl_pop_back_t = typename tl_pop_back<List>::type;

// list replace_back()

/// Creates a new list with all but the last element of `List`
/// and append `T` to the new list.
template <class List, class Back, class Intermediate = type_list<>>
struct tl_replace_back;

template <class T0, class T1, class... Ts, class Back, class... Us>
struct tl_replace_back<type_list<T0, T1, Ts...>, Back, type_list<Us...>>
    : tl_replace_back<type_list<T1, Ts...>, Back, type_list<Us..., T0>> {};

template <class T, class Back, class... Us>
struct tl_replace_back<type_list<T>, Back, type_list<Us...>> {
  using type = type_list<Us..., Back>;
};

template <class List, class Back, class Intermediate = type_list<>>
using tl_replace_back_t
  = typename tl_replace_back<List, Back, Intermediate>::type;

// type at(size_t)

template <size_t N, class... E>
struct tl_at_impl;

template <size_t N, typename E0, class... E>
struct tl_at_impl<N, E0, E...> {
  using type = typename tl_at_impl<N - 1, E...>::type;
};

template <class E0, class... E>
struct tl_at_impl<0, E0, E...> {
  using type = E0;
};

template <size_t N>
struct tl_at_impl<N> {
  using type = unit_t; // no such element
};

template <class List, size_t N>
struct tl_at;

/// Gets element at index `N` of `List`.
template <size_t N, class... E>
struct tl_at<type_list<E...>, N> {
  using type = typename tl_at_impl<N, E...>::type;
};

template <class List, size_t N>
using tl_at_t = typename tl_at<List, N>::type;

// list prepend(type)

template <class List, class What>
struct tl_prepend;

/// Creates a new list with `What` prepended to `List`.
template <class What, class... T>
struct tl_prepend<type_list<T...>, What> {
  using type = type_list<What, T...>;
};

template <class List, class What>
using tl_prepend_t = typename tl_prepend<List, What>::type;

// list filter(predicate)
// list filter_not(predicate)

template <class List, bool... Selected>
struct tl_filter_impl;

template <>
struct tl_filter_impl<empty_type_list> {
  using type = empty_type_list;
};

template <class T0, class... T, bool... S>
struct tl_filter_impl<type_list<T0, T...>, false, S...> {
  using type = typename tl_filter_impl<type_list<T...>, S...>::type;
};

template <class T0, class... T, bool... S>
struct tl_filter_impl<type_list<T0, T...>, true, S...> {
  using type =
    tl_prepend_t<
      typename tl_filter_impl<
        type_list<T...>,
        S...
      >::type,
      T0
    >;
};

template <class List, template <class> class Pred>
struct tl_filter;

/// Create a new list containing all elements which satisfy `Pred`.
template <template <class> class Pred, class... T>
struct tl_filter<type_list<T...>, Pred> {
  using type =
    typename tl_filter_impl<
      type_list<T...>,
      Pred<T>::value...
    >::type;
};

template <class List, template <class> class Pred>
using tl_filter_t = typename tl_filter<List, Pred>::type;

/// Creates a new list containing all elements which
///    do not satisfy `Pred`.
template <class List, template <class> class Pred>
struct tl_filter_not;

template <template <class> class Pred>
struct tl_filter_not<empty_type_list, Pred> {
  using type = empty_type_list;
};

template <template <class> class Pred, class... T>
struct tl_filter_not<type_list<T...>, Pred> {
  using type =
    typename tl_filter_impl<
      type_list<T...>,
      !Pred<T>::value...
    >::type;
};

template <class List, template <class> class Pred>
using tl_filter_not_t = typename tl_filter_not<List, Pred>::type;

/// Creates a new list containing all elements which
///    are equal to `Type`.
template <class List, class Type>
struct tl_filter_type;

template <class Type, class... T>
struct tl_filter_type<type_list<T...>, Type> {
  using type =
    typename tl_filter_impl<
      type_list<T...>,
      !std::is_same<T, Type>::value...
    >::type;
};

template <class List, class T>
using tl_filter_type_t = typename tl_filter_type<List, T>::type;

/// Creates a new list containing all elements which
///    are not equal to `Type`.
template <class List, class Type>
struct tl_filter_not_type;

template <class Type, class... T>
struct tl_filter_not_type<type_list<T...>, Type> {
  using type =
    typename tl_filter_impl<
      type_list<T...>,
      (!std::is_same<T, Type>::value)...
    >::type;
};

template <class List, class T>
using tl_filter_not_type_t = typename tl_filter_not_type<List, T>::type;

// list distinct(list)

/// Creates a new list from `List` without any duplicate elements.
template <class List>
struct tl_distinct;

template <>
struct tl_distinct<empty_type_list> {
  using type = empty_type_list;
};

template <class T0, class... Ts>
struct tl_distinct<type_list<T0, Ts...>> {
  using type =
    tl_concat_t<
      type_list<T0>,
      typename tl_distinct<
        tl_filter_type_t<
          type_list<Ts...>,
          T0
        >
      >::type
    >;
};

template <class List>
using tl_distinct_t = typename tl_distinct<List>::type;

// bool is_distinct

/// Tests whether a list is distinct.
template <class List>
struct tl_is_distinct {
  static constexpr bool value =
    tl_size<List>::value == tl_size<tl_distinct_t<List>>::value;
};

// Uncomment after having switched to C++14
//template <class List>
//inline constexpr bool tl_is_distinct_v = tl_is_distinct<List>::value;

// list resize(list, size, fill_type)

template <class List, bool OldSizeLessNewSize, size_t OldSize, size_t NewSize,
      class FillType>
struct tl_pad_right_impl;

template <class List, size_t OldSize, size_t NewSize, class FillType>
struct tl_pad_right_impl<List, false, OldSize, NewSize, FillType> {
  using type = tl_slice_t<List, 0, NewSize>;
};

template <class List, size_t Size, class FillType>
struct tl_pad_right_impl<List, false, Size, Size, FillType> {
  using type = List;
};

template <class List, size_t OldSize, size_t NewSize, class FillType>
struct tl_pad_right_impl<List, true, OldSize, NewSize, FillType> {
  using type =
    typename tl_pad_right_impl<
      tl_push_back_t<List, FillType>,
      OldSize + 1 < NewSize,
      OldSize + 1,
      NewSize,
      FillType
    >::type;
};

/// Resizes the list to contain `NewSize` elements and uses
///    `FillType` to initialize the new elements with.
template <class List, size_t NewSize, class FillType = unit_t>
struct tl_pad_right {
  using type =
    typename tl_pad_right_impl<
      List,
      (tl_size<List>::value < NewSize),
      tl_size<List>::value,
      NewSize,
      FillType
    >::type;
};

template <class List, size_t NewSize, class FillType = unit_t>
using tl_pad_right_t = typename tl_pad_right<List, NewSize, FillType>::type;

// bool pad_left(list, N)

template <class List, size_t OldSize, size_t NewSize, class FillType>
struct tl_pad_left_impl {
  using type =
    typename tl_pad_left_impl<
      tl_push_front_t<List, FillType>,
      OldSize + 1,
      NewSize,
      FillType
    >::type;
};

template <class List, size_t Size, class FillType>
struct tl_pad_left_impl<List, Size, Size, FillType> {
  using type = List;
};

/// Resizes the list to contain `NewSize` elements and uses
///    `FillType` to initialize prepended elements with.
template <class List, size_t NewSize, class FillType = unit_t>
struct tl_pad_left {
  static constexpr size_t list_size = tl_size<List>::value;
  using type =
    typename tl_pad_left_impl<
      List,
      list_size,
      (list_size > NewSize) ? list_size : NewSize,
      FillType
    >::type;
};

template <class List, size_t NewSize, class FillType = unit_t>
using tl_pad_left_t = typename tl_pad_left<List, NewSize, FillType>::type;

// bool is_zipped(list)

template <class List>
struct tl_is_zipped {
  static constexpr bool value = tl_forall<List, is_type_pair>::value;
};

// Uncomment after having switched to C++14
//template <class List>
//inline constexpr bool tl_is_zipped_v = tl_is_zipped<List>::value;

/// Removes trailing `What` elements from the end.
template <class List, class What = unit_t>
struct tl_trim {
  using type =
    typename std::conditional<
      std::is_same<typename tl_back<List>::type, What>::value,
      typename tl_trim<tl_pop_back_t<List>, What>::type,
      List
    >::type;
};

template <class What>
struct tl_trim<empty_type_list, What> {
  using type = empty_type_list;
};

template <class List, class What = unit_t>
using tl_trim_t = typename tl_trim<List, What>::type;

// list union(list1, list2)

template <class... Ts>
struct tl_union {
  using type = tl_distinct_t<tl_concat_t<Ts...>>;
};

template <class... Ts>
using tl_union_t = typename tl_union<Ts...>::type;

// list intersect(list1, list2)

template <class List,
          bool HeadIsUnique = tl_count_type<List, tl_head_t<List>>::value == 1>
struct tl_intersect_impl;

template <>
struct tl_intersect_impl<empty_type_list, false> {
  using type = empty_type_list;
};

template <class T0, class... Ts>
struct tl_intersect_impl<type_list<T0, Ts...>, true> {
  using type = typename tl_intersect_impl<type_list<Ts...>>::type;
};

template <class T0, class... Ts>
struct tl_intersect_impl<type_list<T0, Ts...>, false> {
  using type =
    tl_concat_t<
      type_list<T0>,
      typename tl_intersect_impl<
        tl_filter_type_t<
          type_list<Ts...>,
          T0
        >
      >::type
    >;
};

template <class... Ts>
struct tl_intersect {
  using type =
    typename tl_intersect_impl<
      tl_concat_t<Ts...>
    >::type;
};

template <class... Ts>
using tl_intersect_t = typename tl_intersect<Ts...>::type;

// list group_by(list, predicate)

template <bool Append, class What, class Where>
struct tl_group_by_impl_step;

template <class What, class... Ts>
struct tl_group_by_impl_step<true, What, type_list<Ts...>> {
  using type = type_list<type_list<Ts..., What>>;
};

template <class What, class List>
struct tl_group_by_impl_step<false, What, List> {
  using type = type_list<List, type_list<What>>;
};

template <class In, class Out, template <class, typename> class Pred>
struct tl_group_by_impl {
  using last_group = tl_back_t<Out>;
  using suffix =
    typename tl_group_by_impl_step<
      Pred<
        tl_head<In>,
        tl_back<last_group>
      >::value,
      tl_head_t<In>,
      last_group
    >::type;
  using prefix = tl_pop_back_t<Out>;
  using new_out = tl_concat_t<prefix, suffix>;
  using type =
    typename tl_group_by_impl<
      tl_tail_t<In>,
      new_out,
      Pred
    >::type;
};

template <template <class, class> class Pred, class T0, class... Ts>
struct tl_group_by_impl<type_list<T0, Ts...>, empty_type_list, Pred> {
  using type =
    typename tl_group_by_impl<
      type_list<Ts...>,
      type_list<type_list<T0>>,
      Pred
    >::type;
};

template <class Out, template <class, typename> class Pred>
struct tl_group_by_impl<empty_type_list, Out, Pred> {
  using type = Out;
};

template <class List, template <class, typename> class Pred>
struct tl_group_by {
  using type =
    typename tl_group_by_impl<
      List,
      empty_type_list,
      Pred
    >::type;
};

template <class List, template <class, typename> class Pred>
using tl_group_by_t = typename tl_group_by<List, Pred>::type;

/// Applies the types of the list to `VarArgTemplate`.
template <class List, template <class...> class VarArgTemplate>
struct tl_apply;

template <class... Ts, template <class...> class VarArgTemplate>
struct tl_apply<type_list<Ts...>, VarArgTemplate> {
  using type = VarArgTemplate<Ts...>;
};

template <class List, template <class...> class VarArgTemplate>
using tl_apply_t = typename tl_apply<List, VarArgTemplate>::type;

// contains(list, x)

template <class List, class X>
struct tl_contains;

template <class X>
struct tl_contains<type_list<>, X> : std::false_type {};

template <class... Ts, class X>
struct tl_contains<type_list<X, Ts...>, X> : std::true_type {};

template <class T, class... Ts, class X>
struct tl_contains<type_list<T, Ts...>, X> : tl_contains<type_list<Ts...>, X> {};

// Uncomment after having switched to C++14
//template <class List, class X>
//inline constexpr bool tl_contains_v = tl_contains<List, X>::value;

// subset_of(list, list)

template <class ListA, class ListB, bool Fwd = true>
struct tl_subset_of;

template <class List>
struct tl_subset_of<type_list<>, List, true> : std::true_type {};

template <class ListA, class ListB>
struct tl_subset_of<ListA, ListB, false> : std::false_type {};

template <class T, class... Ts, class List>
struct tl_subset_of<type_list<T, Ts...>, List>
    : tl_subset_of<type_list<Ts...>, List, tl_contains<List, T>::value> {};

// Uncomment after having switched to C++14
// template <class ListA, class ListB, bool Fwd = true>
// inline constexpr bool tl_subset_of_v = tl_subset_of<ListA, ListB, Fwd>::value;

/// Tests whether ListA contains the same elements as ListB
/// and vice versa. This comparison ignores element positions.
template <class ListA, class ListB>
struct tl_equal {
  static constexpr bool value = tl_subset_of<ListA, ListB>::value
                                && tl_subset_of<ListB, ListA>::value;
};

// Uncomment after having switched to C++14
// template <class ListA, class ListB>
// inline constexpr bool tl_equal_v = tl_equal<ListA, ListB>::value;

template <size_t N, class T>
struct tl_replicate {
  using type = tl_cons_t<T, typename tl_replicate<N - 1, T>::type>;
};

template <class T>
struct tl_replicate<0, T> {
  using type = type_list<>;
};

template <size_t N, class T>
using tl_replicate_t = typename tl_replicate<N, T>::type;

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPE_LIST_HPP
