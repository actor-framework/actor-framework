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

#include <tuple>
#include <string>
#include <utility>
#include <functional>
#include <type_traits>
#include <vector>

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
                                || is_one_of<T, std::string, std::u16string,
                                             std::u32string, atom_value,
                                             message, actor, group,
                                             node_id>::value;
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

/// Checks wheter `T x` allows `x.insert(x.end(), first, last)` where
/// both `first` and `last` have type `const char*`.
template <class T>
class has_char_insert {
  template <class C>
  static bool sfinae_fun(C* cc,
                         const char* first = nullptr,
                         const char* last = nullptr,
                         decltype(cc->insert(cc->end(), first, last))* = 0) {
    return true;
  }

  // SFNINAE default
  static void sfinae_fun(const void*) {
    // nop
  }

  using decayed = typename std::decay<T>::type;

  using result_type = decltype(sfinae_fun(static_cast<decayed*>(nullptr)));

public:
  static constexpr bool value = is_primitive<T>::value == false &&
                  std::is_same<bool, result_type>::value;
};

/// Checks whether `T` has `begin()` and `end()` member
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
  static void sfinae_fun(const void*) {
    // nop
  }

  using decayed = typename std::decay<T>::type;

  using result_type = decltype(sfinae_fun(static_cast<const decayed*>(nullptr)));

public:
  static constexpr bool value = is_primitive<T>::value == false &&
                  std::is_same<bool, result_type>::value;
};

template <class T>
constexpr bool is_iterable<T>::value;

/// Checks whether T is a contiguous sequence of byte.
template <class T>
struct is_byte_sequence : std::false_type { };

template <>
struct is_byte_sequence<std::vector<char>> : std::true_type { };

template <>
struct is_byte_sequence<std::vector<unsigned char>> : std::true_type { };

template <>
struct is_byte_sequence<std::string> : std::true_type { };

/// Checks whether `T` is an `std::tuple` or `std::pair`.
template <class T>
struct is_tuple : std::false_type { };

template <class... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type { };

template <class F, class S>
struct is_tuple<std::pair<F, S>> : std::true_type { };

/// Checks whether `T` provides either a free function or a member function for
/// serialization. The checks test whether both serialization and
/// deserialization can succeed. The meta function tests the following
/// functions with `Processor` being both `serializer` and `deserializer` and
/// returns an integral constant if and only if the test succeeds for both.
///
/// - `serialize(Processor&, T&, const unsigned int)`
/// - `serialize(Processor&, T&)`
/// - `T::serialize(Processor&, const unsigned int)`.
/// - `T::serialize(Processor&)`.
template <class T,
          bool Ignore = std::is_pointer<T>::value
                        || std::is_function<T>::value>
struct has_serialize {
  template <class U>
  static auto test_serialize(caf::serializer* sink, U* x, const unsigned int y = 0)
  -> decltype(serialize(*sink, *x, y));

  template <class U>
  static auto test_serialize(caf::serializer* sink, U* x)
  -> decltype(serialize(*sink, *x));

  template <class>
  static auto test_serialize(...) -> std::false_type;

  template <class U>
  static auto test_deserialize(caf::deserializer* source, U* x, const unsigned int y = 0)
  -> decltype(serialize(*source, *x, y));

  template <class U>
  static auto test_deserialize(caf::deserializer* source, U* x)
  -> decltype(serialize(*source, *x));

  template <class>
  static auto test_deserialize(...) -> std::false_type;

  using serialize_type = decltype(test_serialize<T>(nullptr, nullptr));
  using deserialize_type = decltype(test_deserialize<T>(nullptr, nullptr));
  using type = std::integral_constant<
    bool,
    std::is_same<serialize_type, void>::value
    && std::is_same<deserialize_type, void>::value
  >;

  static constexpr bool value = type::value;
};

template <class T>
struct has_serialize<T, true> {
  static constexpr bool value = false;
};

template <class T>
struct is_serializable;

template <class T,
          bool IsIterable = is_iterable<T>::value,
          bool Ignore = std::is_pointer<T>::value
                        || std::is_function<T>::value>
struct is_serializable_impl;

/// Checks whether `T` is builtin or provides a `serialize`
/// (free or member) function.
template <class T>
struct is_serializable_impl<T, false, false> {
  static constexpr bool value = has_serialize<T>::value
                                || is_builtin<T>::value;
};

template <class F, class S>
struct is_serializable_impl<std::pair<F, S>, false, false> {
  static constexpr bool value = is_serializable<F>::value
                                && is_serializable<S>::value;
};

template <class... Ts>
struct is_serializable_impl<std::tuple<Ts...>, false, false> {
  static constexpr bool value = conjunction<
                                  is_serializable<Ts>::value...
                                >::value;
};

template <class T>
struct is_serializable_impl<T, true, false> {
  using value_type = typename T::value_type;
  static constexpr bool value = is_serializable<value_type>::value;
};

template <class T, size_t S>
struct is_serializable_impl<T[S], false, false> {
  static constexpr bool value = is_serializable<T>::value;
};

template <class T, bool IsIterable>
struct is_serializable_impl<T, IsIterable, true> {
  static constexpr bool value = false;
};

/// Checks whether `T` is builtin or provides a `serialize`
/// (free or member) function.
template <class T>
struct is_serializable {
  static constexpr bool value = is_serializable_impl<T>::value
                                || std::is_empty<T>::value
                                || std::is_enum<T>::value;
};

template <>
struct is_serializable<bool> : std::true_type  {
  // nop
};

template <class T>
struct is_serializable<T&> : is_serializable<T> {
  // nop
};

template <class T>
struct is_serializable<const T> : is_serializable<T> {
  // nop
};

template <class T>
struct is_serializable<const T&> : is_serializable<T> {
  // nop
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

  // picked for any U without requested member since `U::name` is not ambigious
  template <class U>
  static char fun(U*, decltype(U::name)* = nullptr);

  // picked for any U with requested member since `U::name` is ambigious
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
struct has_data_member {
  template <class U>
  static auto test(U* x) -> decltype(x->data(), std::true_type());

  template <class U>
  static auto test(...) -> std::false_type;

  using type = decltype(test<T>(nullptr));
  static constexpr bool value = type::value;
};

template <class T>
struct has_size_member {
  template <class U>
  static auto test(U* x) -> decltype(x->size(), std::true_type());

  template <class U>
  static auto test(...) -> std::false_type;

  using type = decltype(test<T>(nullptr));
  static constexpr bool value = type::value;
};

/// Checks whether T is convertible to either `std::function<void (T&)>`
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

// drops the `const` qualifier in key-value pairs from the STL
template <class T>
struct deconst_kvp {
  using type = T;
};

template <class K, class V>
struct deconst_kvp<std::pair<K, V>> {
  using type = std::pair<typename std::remove_const<K>::type,
                         typename std::remove_const<V>::type>;
};

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

} // namespace detail
} // namespace caf

#endif // CAF_DETAIL_TYPE_TRAITS_HPP
