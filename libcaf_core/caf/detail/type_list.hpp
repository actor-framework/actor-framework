// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/none.hpp"
#include "caf/type_list.hpp"
#include "caf/unit.hpp"

#include <cstddef>
#include <type_traits>
#include <typeinfo>

namespace caf::detail {

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

/// Convenience alias for `is_type_list<T>::value`.
template <class T>
inline constexpr bool is_type_list_v = is_type_list<T>::value;

// T head(type_list)

/// Gets the first element of `List`.
template <class List>
struct tl_head;

template <>
struct tl_head<type_list<>> {
  using type = void;
};

template <class T0, class... Ts>
struct tl_head<type_list<T0, Ts...>> {
  using type = T0;
};

template <class List>
using tl_head_t = typename tl_head<List>::type;

// size_t size(type_list)

/// Gets the number of template parameters of `List`.
template <class List>
struct tl_size;

template <class... Ts>
struct tl_size<type_list<Ts...>> {
  static constexpr size_t value = sizeof...(Ts);
};

/// Convenience alias for `tl_size<List>::value`.
template <class List>
inline constexpr size_t tl_size_v = tl_size<List>::value;

// T back(type_list)

/// Gets the last element in `List`.
template <class List>
struct tl_back;

template <>
struct tl_back<type_list<>> {
  using type = unit_t;
};

template <class T0>
struct tl_back<type_list<T0>> {
  using type = T0;
};

template <class T0, class T1, class... Ts>
struct tl_back<type_list<T0, T1, Ts...>> {
  // remaining arguments are forwarded as type_list to prevent
  // recursive instantiation of List class
  using type = typename tl_back<type_list<T1, Ts...>>::type;
};

template <class List>
using tl_back_t = typename tl_back<List>::type;

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

template <class List, class T>
constexpr int tl_index_of_v = tl_index_of<List, T>::value;

// bool forall(predicate)

/// Tests whether a predicate holds for all elements of a list.
template <class List, template <class> class Pred>
struct tl_forall;

template <class... Ts, template <class> class Pred>
struct tl_forall<type_list<Ts...>, Pred> {
  static constexpr bool value = (Pred<Ts>::value && ...);
};

template <template <class> class Pred>
struct tl_forall<empty_type_list, Pred> {
  static constexpr bool value = true;
};

template <class List, template <class> class Pred>
inline constexpr bool tl_forall_v = tl_forall<List, Pred>::value;

/// Tests whether a predicate holds for some of the elements of a list.
template <class List, template <class> class Pred>
struct tl_exists;

template <class... Ts, template <class> class Pred>
struct tl_exists<type_list<Ts...>, Pred> {
  static constexpr bool value = (Pred<Ts>::value || ...);
};

template <template <class> class Pred>
struct tl_exists<empty_type_list, Pred> {
  static constexpr bool value = false;
};

/// Convenience alias for `tl_exists<List, Pred>::value`.
template <class List, template <class> class Pred>
inline constexpr bool tl_exists_v = tl_exists<List, Pred>::value;

// list map(list, trait)

/// Creates a new list by applying a "template function" to each element.
template <class List, template <class> class Fn>
struct tl_map;

template <class... Ts, template <class> class Fn>
struct tl_map<type_list<Ts...>, Fn> {
  using type = type_list<typename Fn<Ts>::type...>;
};

template <class List, template <class> class... Funs>
using tl_map_t = typename tl_map<List, Funs...>::type;

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
using tl_replace_back_t =
  typename tl_replace_back<List, Back, Intermediate>::type;

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

// list filter(list, predicate)

template <class Input, class Selection, class Output>
struct tl_filter_impl;

template <class... Out>
struct tl_filter_impl<empty_type_list, std::integer_sequence<bool>,
                      type_list<Out...>> {
  using type = type_list<Out...>;
};

template <class T, class... Ts, bool... Vals, class... Out>
struct tl_filter_impl<type_list<T, Ts...>,
                      std::integer_sequence<bool, true, Vals...>,
                      type_list<Out...>>
  : tl_filter_impl<type_list<Ts...>, std::integer_sequence<bool, Vals...>,
                   type_list<Out..., T>> {};

template <class T, class... Ts, bool... Vals, class... Out>
struct tl_filter_impl<type_list<T, Ts...>,
                      std::integer_sequence<bool, false, Vals...>,
                      type_list<Out...>>
  : tl_filter_impl<type_list<Ts...>, std::integer_sequence<bool, Vals...>,
                   type_list<Out...>> {};

template <class List, template <class> class Pred>
struct tl_filter;

/// Create a new list containing all elements which satisfy `Pred`.
template <class... Ts, template <class> class Pred>
struct tl_filter<type_list<Ts...>, Pred>
  : tl_filter_impl<type_list<Ts...>,
                   std::integer_sequence<bool, Pred<Ts>::value...>,
                   type_list<>> {};

template <class List, template <class> class Pred>
using tl_filter_t = typename tl_filter<List, Pred>::type;

// list filter_not(list, predicate)

template <class Input, class Selection, class Output>
struct tl_filter_not_impl;

template <class... Out>
struct tl_filter_not_impl<empty_type_list, std::integer_sequence<bool>,
                          type_list<Out...>> {
  using type = type_list<Out...>;
};

template <class T, class... Ts, bool... Vals, class... Out>
struct tl_filter_not_impl<type_list<T, Ts...>,
                          std::integer_sequence<bool, true, Vals...>,
                          type_list<Out...>>
  : tl_filter_not_impl<type_list<Ts...>, std::integer_sequence<bool, Vals...>,
                       type_list<Out...>> {};

template <class T, class... Ts, bool... Vals, class... Out>
struct tl_filter_not_impl<type_list<T, Ts...>,
                          std::integer_sequence<bool, false, Vals...>,
                          type_list<Out...>>
  : tl_filter_not_impl<type_list<Ts...>, std::integer_sequence<bool, Vals...>,
                       type_list<Out..., T>> {};

template <class List, template <class> class Pred>
struct tl_filter_not;

/// Create a new list containing all elements which satisfy `Pred`.
template <class... Ts, template <class> class Pred>
struct tl_filter_not<type_list<Ts...>, Pred>
  : tl_filter_not_impl<type_list<Ts...>,
                       std::integer_sequence<bool, Pred<Ts>::value...>,
                       type_list<>> {};

template <class List, template <class> class Pred>
using tl_filter_not_t = typename tl_filter_not<List, Pred>::type;

// bool is_distinct(list)

template <class List>
struct tl_is_distinct;

template <>
struct tl_is_distinct<empty_type_list> : std::true_type {};

template <class T1, class T2, class... Ts>
struct tl_is_distinct<type_list<T1, T2, Ts...>> {
  static constexpr bool value = !std::is_same_v<T1, T2>
                                && (!std::is_same_v<T1, Ts> && ...)
                                && tl_is_distinct<type_list<T2, Ts...>>::value;
};

/// Tests whether a list is distinct.
template <class List>
struct tl_is_distinct {
  static constexpr bool value = tl_is_distinct<List>::value;
};

template <class List>
inline constexpr bool tl_is_distinct_v = tl_is_distinct<List>::value;

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

template <class List, class T>
struct tl_contains;

template <class... Ts, class T>
struct tl_contains<type_list<Ts...>, T> {
  static constexpr bool value = (std::is_same_v<Ts, T> || ...);
};

/// Convenience alias for `tl_contains<List, X>::value`.
template <class List, class X>
inline constexpr bool tl_contains_v = tl_contains<List, X>::value;

// subset_of(list, list)

/// Evaluates to `true` if `ListA` is a subset of `ListB`, i.e., if all elements
/// in `ListA` are also in `ListB`.
template <class ListA, class ListB>
struct tl_subset_of;

template <class... Ts, class List>
struct tl_subset_of<type_list<Ts...>, List> {
  static constexpr bool value = (tl_contains_v<List, Ts> && ...);
};

/// Convenience alias for `tl_contains<List, X>::value`.
template <class ListA, class ListB>
inline constexpr bool tl_subset_of_v = tl_subset_of<ListA, ListB>::value;

} // namespace caf::detail
