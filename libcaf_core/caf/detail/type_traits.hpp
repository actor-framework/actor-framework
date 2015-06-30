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

#ifndef CAF_DETAIL_TYPE_TRAITS_HPP
#define CAF_DETAIL_TYPE_TRAITS_HPP

#include <string>
#include <functional>
#include <type_traits>

#include "caf/optional.hpp"

#include "caf/fwd.hpp"

#include "caf/detail/type_list.hpp"

namespace caf {
namespace detail {

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

/// Equal to std::is_same<T, anything>.
template <class T>
struct is_anything : std::is_same<T, anything> {};

/// Checks whether `T` is an array of type `U`.
template <class T, typename U>
struct is_array_of {
  using step1_type = typename std::remove_all_extents<T>::type;
  using step2_type = typename std::remove_cv<step1_type>::type;
  static constexpr bool value = std::is_array<T>::value
                                && std::is_same<step2_type, U>::value;
};

/// Deduces the reference type of T0 and applies it to T1.
template <class T0, typename T1>
struct deduce_ref_type {
  using type = typename std::decay<T1>::type;
};

template <class T0, typename T1>
struct deduce_ref_type<T0&, T1> {
  using type = typename std::decay<T1>::type&;
};

template <class T0, typename T1>
struct deduce_ref_type<const T0&, T1> {
  using type = const typename std::decay<T1>::type&;
};

/// Checks wheter `X` is in the template parameter pack Ts.
template <class X, class... Ts>
struct is_one_of;

template <class X>
struct is_one_of<X> : std::false_type {};

template <class X, class... Ts>
struct is_one_of<X, X, Ts...> : std::true_type {};

template <class X, typename T0, class... Ts>
struct is_one_of<X, T0, Ts...> : is_one_of<X, Ts...> {};

/// Checks wheter `T` is considered a builtin type.
///
/// Builtin types are: (1) all arithmetic types, (2) string types from the STL,
/// and (3) built-in types such as `actor_ptr`.
template <class T>
struct is_builtin {
  // all arithmetic types are considered builtin
  static constexpr bool value = std::is_arithmetic<T>::value
                                || is_one_of<T, anything, std::string,
                                             std::u16string, std::u32string,
                                             atom_value, message, actor, group,
                                             channel, node_id>::value;
};

/// Chekcs wheter `T` is primitive, i.e., either an arithmetic
///    type or convertible to one of STL's string types.
template <class T>
struct is_primitive {
  static constexpr bool value = std::is_arithmetic<T>::value
                                || std::is_convertible<T, std::string>::value
                                || std::is_convertible<T, std::u16string>::value
                                || std::is_convertible<T, std::u32string>::value
                                || std::is_convertible<T, atom_value>::value;
};

/// Chekcs wheter `T1` is comparable with `T2`.
template <class T1, typename T2>
class is_comparable {
  // SFINAE: If you pass a "bool*" as third argument, then
  //     decltype(cmp_help_fun(...)) is bool if there's an
  //     operator==(A, B) because
  //     cmp_help_fun(A*, B*, bool*) is a better match than
  //     cmp_help_fun(A*, B*, void*). If there's no operator==(A, B)
  //     available, then cmp_help_fun(A*, B*, void*) is the only
  //     candidate and thus decltype(cmp_help_fun(...)) is void.
  template <class A, typename B>
  static bool cmp_help_fun(const A* arg0, const B* arg1,
               decltype(*arg0 == *arg1)* = nullptr) {
    return true;
  }
  template <class A, typename B>
  static void cmp_help_fun(const A*, const B*, void* = nullptr) { }

  using result_type = decltype(cmp_help_fun(static_cast<T1*>(nullptr),
                                            static_cast<T2*>(nullptr),
                                            static_cast<bool*>(nullptr)));
public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks wheter `T` behaves like a forward iterator.
template <class T>
class is_forward_iterator {
  template <class C>
  static bool sfinae_fun(
    C* iter,
    // check for 'C::value_type C::operator*()' returning a non-void type
    typename std::decay<decltype(*(*iter))>::type* = 0,
    // check for 'C& C::operator++()'
    typename std::enable_if<
      std::is_same<C&, decltype(++(*iter))>::value>::type* = 0,
    // check for 'bool C::operator==()'
    typename std::enable_if<
      std::is_same<bool, decltype(*iter == *iter)>::value>::type* = 0,
    // check for 'bool C::operator!=()'
    typename std::enable_if<
      std::is_same<bool, decltype(*iter != *iter)>::value>::type* = 0) {
    return true;
  }

  static void sfinae_fun(void*) {}

  using result_type = decltype(sfinae_fun(static_cast<T*>(nullptr)));

public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks wheter `T` has `begin()` and `end()` member
/// functions returning forward iterators.
template <class T>
class is_iterable {
  // this horrible code would just disappear if we had concepts
  template <class C>
  static bool sfinae_fun(
    const C* cc,
    // check for 'C::begin()' returning a forward iterator
    typename std::enable_if<is_forward_iterator<decltype(cc->begin())>::value>::
      type* = 0,
    // check for 'C::end()' returning the same kind of forward iterator
    typename std::enable_if<std::is_same<decltype(cc->begin()),
                                         decltype(cc->end())>::value>::type
    * = 0) {
    return true;
  }

  // SFNINAE default
  static void sfinae_fun(const void*) {}

  using result_type = decltype(sfinae_fun(static_cast<const T*>(nullptr)));

public:
  static constexpr bool value = is_primitive<T>::value == false &&
                  std::is_same<bool, result_type>::value;
};

template<class T>
constexpr bool is_iterable<T>::value;

/// Checks wheter `T` is neither a reference nor a pointer nor an array.
template <class T>
struct is_legal_tuple_type {
  static constexpr bool value = std::is_reference<T>::value == false
                                && std::is_pointer<T>::value == false
                                && std::is_array<T>::value == false;

};

/// Checks wheter `T` is a non-const reference.
template <class T>
struct is_mutable_ref : std::false_type { };

template <class T>
struct is_mutable_ref<const T&> : std::false_type { };

template <class T>
struct is_mutable_ref<T&> : std::true_type { };

/// Checks whether `T::static_type_name()` exists.
template <class T>
class has_static_type_name {
private:
  template <class U,
            class = typename std::enable_if<
                      ! std::is_member_pointer<decltype(&U::static_type_name)>::value
                    >::type>
  static std::true_type sfinae_fun(int);
  template <class>
  static std::false_type sfinae_fun(...);
public:
  static constexpr bool value = decltype(sfinae_fun<T>(0))::value;
};

/// Checks whether `T::memory_cache_flag` exists.
template <class T>
class is_memory_cached {
private:
  template <class U, bool = U::memory_cache_flag>
  static std::true_type check(int);

  template <class>
  static std::false_type check(...);

public:
  static constexpr bool value = decltype(check<T>(0))::value;
};

/// Returns either `T` or `T::type` if `T` is an option.
template <class T>
struct rm_optional {
  using type = T;
};

template <class T>
struct rm_optional<optional<T>> {
  using type = T;
};

/// Defines `result_type,` `arg_types,` and `fun_type`. Functor is
///    (a) a member function pointer, (b) a function,
///    (c) a function pointer, (d) an std::function.
///
/// `result_type` is the result type found in the signature.
/// `arg_types` are the argument types as {@link type_list}.
/// `fun_type` is an std::function with an equivalent signature.
template <class Functor>
struct callable_trait;

// member const function pointer
template <class C, typename Result, class... Ts>
struct callable_trait<Result (C::*)(Ts...) const> {
  using result_type = Result;
  using arg_types = type_list<Ts...>;
  using fun_type = std::function<Result(Ts...)>;
};

// member function pointer
template <class C, typename Result, class... Ts>
struct callable_trait<Result (C::*)(Ts...)> {
  using result_type = Result;
  using arg_types = type_list<Ts...>;
  using fun_type = std::function<Result(Ts...)>;
};

// good ol' function
template <class Result, class... Ts>
struct callable_trait<Result(Ts...)> {
  using result_type = Result;
  using arg_types = type_list<Ts...>;
  using fun_type = std::function<Result(Ts...)>;
};

// good ol' function pointer
template <class Result, class... Ts>
struct callable_trait<Result (*)(Ts...)> {
  using result_type = Result;
  using arg_types = type_list<Ts...>;
  using fun_type = std::function<Result(Ts...)>;
};

// matches (IsFun || IsMemberFun)
template <bool IsFun, bool IsMemberFun, typename T>
struct get_callable_trait_helper {
  using type = callable_trait<T>;
};

// assume functor providing operator()
template <class C>
struct get_callable_trait_helper<false, false, C> {
  using type = callable_trait<decltype(&C::operator())>;
};

/// Gets a callable trait for `T,` where `T` is a functor type,
///    i.e., a function, member function, or a class providing
///    the call operator.
template <class T>
struct get_callable_trait {
  // type without cv qualifiers
  using bare_type = typename std::decay<T>::type;
  // if T is a function pointer, this type identifies the function
  using signature_type = typename std::remove_pointer<bare_type>::type;
  using type =
    typename get_callable_trait_helper<
      std::is_function<bare_type>::value
      || std::is_function<signature_type>::value,
      std::is_member_function_pointer<bare_type>::value,
      bare_type
    >::type;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  static constexpr size_t num_args = tl_size<arg_types>::value;
};

/// Checks wheter `T` is a function or member function.
template <class T>
struct is_callable {
  template <class C>
  static bool _fun(C*, typename callable_trait<C>::result_type* = nullptr) {
    return true;
  }

  template <class C>
  static bool
  _fun(C*,
     typename callable_trait<decltype(&C::operator())>::result_type* = 0) {
    return true;
  }

  static void _fun(void*) { }

  using pointer = typename std::decay<T>::type*;

  using result_type = decltype(_fun(static_cast<pointer>(nullptr)));

public:
  static constexpr bool value = std::is_same<bool, result_type>::value;
};

/// Checks wheter each `T` in `Ts` is a function or member function.
template <class... Ts>
struct all_callable {
  static constexpr bool value = conjunction<is_callable<Ts>::value...>::value;
};

/// Checks wheter `F` takes mutable references.
///
/// A manipulator is a functor that manipulates its arguments via
/// mutable references.
template <class F>
struct is_manipulator {
  static constexpr bool value =
    tl_exists<typename get_callable_trait<F>::arg_types, is_mutable_ref>::value;
};

template <bool IsCallable, typename C>
struct map_to_result_type_impl {
  using trait_type = typename get_callable_trait<C>::type;
  using type = typename trait_type::result_type;
};

template <class C>
struct map_to_result_type_impl<false, C> {
  using type = unit_t;
};

/// Maps `T` to its result type if it's callable,
///    {@link unit_t} otherwise.
template <class T>
struct map_to_result_type {
  using type =
    typename map_to_result_type_impl<
      is_callable<T>::value,
      T
    >::type;
};

template <bool DoReplace, typename T1, typename T2>
struct replace_type_impl {
  using type = T1;
};

template <class T1, typename T2>
struct replace_type_impl<true, T1, T2> {
  using type = T2;
};

template <class T>
constexpr bool value_of() {
  return T::value;
}

/// Replaces `What` with `With` if any IfStmt::value evaluates to true.
template <class What, typename With, class... IfStmt>
struct replace_type {
  static constexpr bool do_replace = disjunction<value_of<IfStmt>()...>::value;
  using type = typename replace_type_impl<do_replace, What, With>::type;
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

template <class T>
struct is_optional : std::false_type {
  // no members
};

template <class T>
struct is_optional<optional<T>> : std::true_type {
  // no members
};

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPE_TRAITS_HPP
