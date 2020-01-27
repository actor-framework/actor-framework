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

#include <array>
#include <chrono>
#include <functional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "caf/fwd.hpp"
#include "caf/timestamp.hpp"

#include "caf/detail/is_one_of.hpp"
#include "caf/detail/type_list.hpp"

#define CAF_HAS_MEMBER_TRAIT(name)                                             \
  template <class T>                                                           \
  class has_##name##_member {                                                  \
  private:                                                                     \
    template <class U>                                                         \
    static auto sfinae(U* x) -> decltype(x->name(), std::true_type());         \
                                                                               \
    template <class U>                                                         \
    static auto sfinae(...) -> std::false_type;                                \
                                                                               \
    using sfinae_type = decltype(sfinae<T>(nullptr));                          \
                                                                               \
  public:                                                                      \
    static constexpr bool value = sfinae_type::value;                          \
  }

#define CAF_HAS_ALIAS_TRAIT(name)                                              \
  template <class T>                                                           \
  class has_##name##_alias {                                                   \
  private:                                                                     \
    template <class C>                                                         \
    static std::true_type sfinae(typename C::name*);                           \
                                                                               \
    template <class>                                                           \
    static std::false_type sfinae(...);                                        \
                                                                               \
    using sfinae_type = decltype(sfinae<T>(nullptr));                          \
                                                                               \
  public:                                                                      \
    static constexpr bool value = sfinae_type::value;                          \
  }

namespace caf::detail {

// -- backport of C++14 additions ----------------------------------------------

template <class T>
using decay_t = typename std::decay<T>::type;

template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;

template <bool V, class T = void>
using enable_if_t = typename std::enable_if<V, T>::type;

// -- custom traits ------------------------------------------------------------

template <class Trait, class T = void>
using enable_if_tt = typename std::enable_if<Trait::value, T>::type;

template <class T>
using remove_reference_t = typename std::remove_reference<T>::type;

/// Checks whether `T` defines a free function `to_string`.
template <class T>
class has_to_string {
private:
  template <class U>
  static auto sfinae(const U& x) -> decltype(to_string(x));

  static void sfinae(...);

  using result = decltype(sfinae(std::declval<const T&>()));

public:
  static constexpr bool value = std::is_convertible<result, std::string>::value;
};

template <bool X>
using bool_token = std::integral_constant<bool, X>;

template <int X>
using int_token = std::integral_constant<int, X>;

/// Joins all bool constants using operator &&.
template <bool... BoolConstants>
struct conjunction;

template <>
struct conjunction<> {
  static constexpr bool value = true;
};

template <bool X, bool... Xs>
struct conjunction<X, Xs...> {
  static constexpr bool value = X && conjunction<Xs...>::value;
};

/// Joins all bool constants using operator ||.
template <bool... BoolConstants>
struct disjunction;

template <>
struct disjunction<> {
  static constexpr bool value = false;
};

template <bool X, bool... Xs>
struct disjunction<X, Xs...> {
  static constexpr bool value = X || disjunction<Xs...>::value;
};

/// Checks whether `T` is a `std::chrono::duration`.
template <class T>
struct is_duration : std::false_type {};

template <class Period, class Rep>
struct is_duration<std::chrono::duration<Period, Rep>> : std::true_type {};

/// Checks whether `T` is primitive, i.e., either an arithmetic type or
/// convertible to one of STL's string types.
template <class T>
struct is_primitive {
  static constexpr bool value = std::is_convertible<T, std::string>::value
                                || std::is_convertible<T, std::u16string>::value
                                || std::is_convertible<T, std::u32string>::value
                                || std::is_arithmetic<T>::value;
};

/// Checks whether `T1` is comparable with `T2`.
template <class T1, class T2>
class is_comparable {
  // SFINAE: If you pass a "bool*" as third argument, then
  //     decltype(cmp_help_fun(...)) is bool if there's an
  //     operator==(A, B) because
  //     cmp_help_fun(A*, B*, bool*) is a better match than
  //     cmp_help_fun(A*, B*, void*). If there's no operator==(A, B)
  //     available, then cmp_help_fun(A*, B*, void*) is the only
  //     candidate and thus decltype(cmp_help_fun(...)) is void.
  template <class A, class B>
  static bool cmp_help_fun(const A* arg0, const B* arg1,
                           decltype(*arg0 == *arg1)*,
                           std::integral_constant<bool, false>);

  // silences float-equal warnings caused by decltype(*arg0 == *arg1)
  template <class A, class B>
  static bool cmp_help_fun(const A*, const B*, bool*,
                           std::integral_constant<bool, true>);

  template <class A, class B, class C>
  static void cmp_help_fun(const A*, const B*, void*, C);

  using result_type = decltype(cmp_help_fun(
    static_cast<T1*>(nullptr), static_cast<T2*>(nullptr),
    static_cast<bool*>(nullptr),
    std::integral_constant<bool, std::is_arithmetic<T1>::value
                                   && std::is_arithmetic<T2>::value>{}));

public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks whether `T` behaves like a forward iterator.
template <class T>
class is_forward_iterator {
  template <class C>
  static bool sfinae(C& x, C& y,
                     // check for operator*
                     decay_t<decltype(*x)>* = nullptr,
                     // check for operator++ returning an iterator
                     decay_t<decltype(x = ++y)>* = nullptr,
                     // check for operator==
                     decay_t<decltype(x == y)>* = nullptr,
                     // check for operator!=
                     decay_t<decltype(x != y)>* = nullptr);

  static void sfinae(...);

  using result_type = decltype(sfinae(std::declval<T&>(), std::declval<T&>()));

public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks whether `T` has `begin()` and `end()` member
/// functions returning forward iterators.
template <class T>
class is_iterable {
  // this horrible code would just disappear if we had concepts
  template <class C>
  static bool sfinae(C* cc,
                     // check if 'C::begin()' returns a forward iterator
                     enable_if_tt<is_forward_iterator<decltype(cc->begin())>>* = nullptr,
                     // check if begin() and end() both exist and are comparable
                     decltype(cc->begin() != cc->end())* = nullptr);

  // SFNINAE default
  static void sfinae(void*);

  using result_type = decltype(sfinae(static_cast<decay_t<T>*>(nullptr)));

public:
  static constexpr bool value = is_primitive<T>::value == false
                                && std::is_same<bool, result_type>::value;
};

template <class T>
constexpr bool is_iterable<T>::value;

/// Checks whether `T` is a non-const reference.
template <class T>
struct is_mutable_ref : std::false_type { };

template <class T>
struct is_mutable_ref<const T&> : std::false_type { };

template <class T>
struct is_mutable_ref<T&> : std::true_type { };

/// Defines `result_type,` `arg_types,` and `fun_type`. Functor is
///    (a) a member function pointer, (b) a function,
///    (c) a function pointer, (d) an std::function.
///
/// `result_type` is the result type found in the signature.
/// `arg_types` are the argument types as {@link type_list}.
/// `fun_type` is an std::function with an equivalent signature.
template <class Functor>
struct callable_trait;

// good ol' function
template <class R, class... Ts>
struct callable_trait<R (Ts...)> {
  using result_type = R;
  using arg_types = type_list<Ts...>;
  using fun_sig = R (Ts...);
  using fun_type = std::function<R (Ts...)>;
  static constexpr size_t num_args = sizeof...(Ts);
};

// member noexcept const function pointer
template <class C, typename R, class... Ts>
struct callable_trait<R (C::*)(Ts...) const noexcept>
  : callable_trait<R(Ts...)> {};

// member noexcept function pointer
template <class C, typename R, class... Ts>
struct callable_trait<R (C::*)(Ts...) noexcept> : callable_trait<R(Ts...)> {};

// member const function pointer
template <class C, typename R, class... Ts>
struct callable_trait<R (C::*)(Ts...) const> : callable_trait<R (Ts...)> {};

// member function pointer
template <class C, typename R, class... Ts>
struct callable_trait<R (C::*)(Ts...)> : callable_trait<R (Ts...)> {};

// good ol' noexcept function pointer
template <class R, class... Ts>
struct callable_trait<R (*)(Ts...) noexcept> : callable_trait<R(Ts...)> {};

// good ol' function pointer
template <class R, class... Ts>
struct callable_trait<R (*)(Ts...)> : callable_trait<R (Ts...)> {};

template <class T>
struct has_apply_operator {
  template <class U>
  static auto sfinae(U*) -> decltype(&U::operator(), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));
  static constexpr bool value = type::value;
};

// matches (IsFun || IsMemberFun)
template <class T,
          bool IsFun = std::is_function<T>::value
                       || std::is_function<
                            typename std::remove_pointer<T>::type
                          >::value
                       || std::is_member_function_pointer<T>::value,
          bool HasApplyOp = has_apply_operator<T>::value>
struct get_callable_trait_helper {
  using type = callable_trait<T>;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  using fun_sig = typename type::fun_sig;
  static constexpr size_t num_args = tl_size<arg_types>::value;
};

// assume functor providing operator()
template <class T>
struct get_callable_trait_helper<T, false, true> {
  using type = callable_trait<decltype(&T::operator())>;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  using fun_sig = typename type::fun_sig;
  static constexpr size_t num_args = tl_size<arg_types>::value;
};

template <class T>
struct get_callable_trait_helper<T, false, false> {};

/// Gets a callable trait for `T,` where `T` is a function object type,
/// i.e., a function, member function, or a class providing
/// the call operator.
template <class T>
struct get_callable_trait : get_callable_trait_helper<decay_t<T>> {};

template <class T>
using get_callable_trait_t = typename get_callable_trait<T>::type;

/// Checks whether `T` is a function or member function.
template <class T>
struct is_callable {
  template <class C>
  static bool _fun(C*, typename get_callable_trait<C>::type* = nullptr);

  static void _fun(void*);

  using result_type = decltype(_fun(static_cast<decay_t<T>*>(nullptr)));

public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks whether `F` is callable with arguments of types `Ts...`.
template <class F, class... Ts>
struct is_callable_with {
  template <class U>
  static auto sfinae(U*)
  -> decltype((std::declval<U&>())(std::declval<Ts>()...), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<F>(nullptr));
  static constexpr bool value = type::value;
};

/// Checks whether `F` takes mutable references.
///
/// A manipulator is a functor that manipulates its arguments via
/// mutable references.
template <class F>
struct is_manipulator {
  static constexpr bool value =
    tl_exists<typename get_callable_trait<F>::arg_types, is_mutable_ref>::value;
};

/// Gets the Nth element of the template parameter pack `Ts`.
template <size_t N, class... Ts>
struct type_at;

template <size_t N, typename T0, class... Ts>
struct type_at<N, T0, Ts...> {
  using type = typename type_at<N - 1, Ts...>::type;
};

template <class T0, class... Ts>
struct type_at<0, T0, Ts...> {
  using type = T0;
};

// Checks whether T has a member variable named `name`.
template <class T, bool IsScalar = std::is_scalar<T>::value>
class has_name {
private:
  // a simple struct with a member called `name`
  struct fallback {
    int name;
  };

  // creates an ambiguity for any `T` with the requested member
  struct derived : T, fallback {
    // no members
  };

  // picked for any U without requested member since `U::name` is not ambiguous
  template <class U>
  static char fun(U*, decltype(U::name)* = nullptr);

  // picked for any U with requested member since `U::name` is ambiguous
  static int fun(void*);

public:
  static constexpr bool value = sizeof(fun(static_cast<derived*>(nullptr))) > 1;
};

template <class T>
class has_name<T, true> {
public:
  static constexpr bool value = false;
};

template <class T>
class has_peek_all {
private:
  template <class U>
  static int fun(const U*,
                 decltype(std::declval<U&>().peek_all(unit))* = nullptr);

  static char fun(const void*);

public:
  static constexpr bool value = sizeof(fun(static_cast<T*>(nullptr))) > 1;
};

CAF_HAS_MEMBER_TRAIT(size);
CAF_HAS_MEMBER_TRAIT(data);

/// Checks whether F is convertible to either `std::function<void (T&)>`
/// or `std::function<void (const T&)>`.
template <class F, class T>
struct is_handler_for {
  static constexpr bool value =
      std::is_convertible<F, std::function<void (T&)>>::value
      || std::is_convertible<F, std::function<void (const T&)>>::value;
};

template <class T>
struct value_type_of {
  using type = typename T::value_type;
};

template <class T>
struct value_type_of<T*> {
  using type = T;
};

template <class T>
using value_type_of_t = typename value_type_of<T>::type;

template <class T>
using is_callable_t = typename std::enable_if<is_callable<T>::value>::type;

template <class F, class T>
using is_handler_for_ef =
  typename std::enable_if<is_handler_for<F, T>::value>::type;

template <class T>
struct strip_reference_wrapper {
  using type = T;
};

template <class T>
struct strip_reference_wrapper<std::reference_wrapper<T>> {
  using type = T;
};

template <class T>
constexpr bool can_insert_elements_impl(
  T*,
  decltype(std::declval<T&>()
           .insert(std::declval<T&>().end(),
                   std::declval<typename T::value_type>()))* = nullptr) {
  return true;
}

template <class T>
constexpr bool can_insert_elements_impl(void*) {
  return false;
}

template <class T>
constexpr bool can_insert_elements() {
  return can_insert_elements_impl<T>(static_cast<T*>(nullptr));
}

/// Checks whether `Tpl` is a specialization of `T` or not.
template <template <class...> class Tpl, class T>
struct is_specialization : std::false_type { };

template <template <class...> class T, class... Ts>
struct is_specialization<T, T<Ts...>> : std::true_type { };

/// Transfers const from `T` to `U`. `U` remains unchanged if `T` is not const.
template <class T, class U>
struct transfer_const {
  using type = U;
};

template <class T, class U>
struct transfer_const<const T, U> {
  using type = const U;
};

template <class T, class U>
using transfer_const_t = typename transfer_const<T, U>::type;

template <class T>
struct is_stream : std::false_type {};

template <class T>
struct is_stream<stream<T>> : std::true_type {};

template <class T>
struct is_result : std::false_type {};

template <class T>
struct is_result<result<T>> : std::true_type {};

template <class T>
struct is_expected : std::false_type {};

template <class T>
struct is_expected<expected<T>> : std::true_type {};

// Checks whether `T` and `U` are integers of the same size and signedness.
// clang-format off
template <class T, class U,
          bool Enable = std::is_integral<T>::value
                        && std::is_integral<U>::value
                        && !std::is_same<T, bool>::value
                        && !std::is_same<U, bool>::value>
// clang-format on
struct is_equal_int_type {
  static constexpr bool value = sizeof(T) == sizeof(U)
                                && std::is_signed<T>::value
                                   == std::is_signed<U>::value;
};

template <class T, typename U>
struct is_equal_int_type<T, U, false> : std::false_type { };

/// Compares `T` to `U` und evaluates to `true_type` if either
/// `T == U or if T and U are both integral types of the
/// same size and signedness. This works around the issue that
/// `uint8_t != unsigned char on some compilers.
template <class T, typename U>
struct is_same_ish
    : std::conditional<
        std::is_same<T, U>::value,
        std::true_type,
        is_equal_int_type<T, U>
      >::type { };

/// Utility for fallbacks calling `static_assert`.
template <class>
struct always_false : std::false_type {};

/// Utility trait for removing const inside a `map<K, V>::value_type`.
template <class T>
struct deconst_kvp {
  using type = T;
};

template <class K, class V>
struct deconst_kvp<std::pair<const K, V>> {
  using type = std::pair<K, V>;
};

template <class T>
using deconst_kvp_t = typename deconst_kvp<T>::type;

/// Utility trait for checking whether T is a `std::pair`.
template <class T>
struct is_pair : std::false_type {};

template <class First, class Second>
struct is_pair<std::pair<First, Second>> : std::true_type {};

template <class T>
constexpr bool is_pair_v = is_pair<T>::value;

// -- traits to check for STL-style type aliases -------------------------------

CAF_HAS_ALIAS_TRAIT(value_type);

CAF_HAS_ALIAS_TRAIT(key_type);

CAF_HAS_ALIAS_TRAIT(mapped_type);

// -- constexpr functions for use in enable_if & friends -----------------------

template <class List1, class List2>
struct all_constructible : std::false_type {};

template <>
struct all_constructible<type_list<>, type_list<>> : std::true_type {};

template <class T, class... Ts, class U, class... Us>
struct all_constructible<type_list<T, Ts...>, type_list<U, Us...>> {
  static constexpr bool value = std::is_constructible<T, U>::value
                                && all_constructible<type_list<Ts...>,
                                                     type_list<Us...>>::value;
};

/// Checks whether T behaves like `std::map`.
template <class T>
struct is_map_like {
  static constexpr bool value = is_iterable<T>::value
                                && has_key_type_alias<T>::value
                                && has_mapped_type_alias<T>::value;
};

template <class T>
constexpr bool is_map_like_v = is_map_like<T>::value;

/// Checks whether T behaves like `std::vector`, `std::list`, or `std::set`.
template <class T>
struct is_list_like {
  static constexpr bool value = is_iterable<T>::value
                                && has_value_type_alias<T>::value
                                && !has_mapped_type_alias<T>::value;
};

template <class T>
constexpr bool is_list_like_v = is_list_like<T>::value;

template <class F, class... Ts>
struct is_invocable {
private:
  template <class U>
  static auto sfinae(U* f)
    -> decltype((*f)(std::declval<Ts>()...), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<F>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class R, class F, class... Ts>
struct is_invocable_r {
private:
  template <class U>
  static auto sfinae(U* f)
    -> std::is_same<R, decltype((*f)(std::declval<Ts>()...))>;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<F>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T, class To>
class has_convertible_data_member {
private:
  template <class U>
  static auto sfinae(U* x) -> std::integral_constant<
    bool, std::is_convertible<decltype(x->data()), To*>::value>;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T, class Arg>
struct can_apply {
  template <class U>
  static auto sfinae(U* x)
    -> decltype(x->apply(std::declval<Arg>()), std::true_type{});

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));

  static constexpr bool value = type::value;
};

template <class T, class Arg>
constexpr bool can_apply_v = can_apply<T, Arg>::value;

/// Evaluates to `true` for all types that specialize `std::tuple_size`, i.e.,
/// `std::tuple`, `std::pair`, and `std::array`.
template <class T>
struct is_stl_tuple_type {
  template <class U>
  static auto sfinae(U*)
    -> decltype(std::integral_constant<bool, std::tuple_size<U>::value >= 0>{});

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));

  static constexpr bool value = type::value;
};

template <class T>
constexpr bool is_stl_tuple_type_v = is_stl_tuple_type<T>::value;

} // namespace caf::detail

#undef CAF_HAS_MEMBER_TRAIT
#undef CAF_HAS_ALIAS_TRAIT
