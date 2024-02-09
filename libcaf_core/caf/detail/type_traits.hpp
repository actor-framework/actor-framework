// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/config.hpp"
#include "caf/detail/is_complete.hpp"
#include "caf/detail/is_one_of.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/timestamp.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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
  };                                                                           \
  template <class T>                                                           \
  inline constexpr bool has_##name##_member_v = has_##name##_member<T>::value

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
  };                                                                           \
  template <class T>                                                           \
  inline constexpr bool has_##name##_alias_v = has_##name##_alias<T>::value

namespace caf::detail {

template <class T>
constexpr T* null_v = nullptr;

// -- custom traits ------------------------------------------------------------

/// Checks whether `T` is a `stream` or `typed_stream`.
template <class T>
struct is_stream : std::false_type {};

template <>
struct is_stream<stream> : std::true_type {};

template <class T>
struct is_stream<typed_stream<T>> : std::true_type {};

template <class T>
inline constexpr bool is_stream_v = is_stream<T>::value;

/// Checks whether  `T` is a `behavior` or `typed_behavior`.
template <class T>
struct is_behavior : std::false_type {};

template <>
struct is_behavior<behavior> : std::true_type {};

template <class... Ts>
struct is_behavior<typed_behavior<Ts...>> : std::true_type {};

template <class T>
inline constexpr bool is_behavior_v = is_behavior<T>::value;

/// Checks whether `T` is a `publisher`.
template <class T>
struct is_publisher : std::false_type {};

template <class T>
struct is_publisher<async::publisher<T>> : std::true_type {};

template <class T>
inline constexpr bool is_publisher_v = is_publisher<T>::value;

/// Checks whether `T` defines a free function `to_string`.
template <class T>
class has_to_string {
private:
  template <class U>
  static auto sfinae(const U& x) -> decltype(to_string(x));

  static void sfinae(...);

  using result = decltype(sfinae(std::declval<const T&>()));

public:
  static constexpr bool value = std::is_convertible_v<result, std::string>;
};

/// Convenience alias for `has_to_string<T>::value`.
/// @relates has_to_string
template <class T>
inline constexpr bool has_to_string_v = has_to_string<T>::value;

/// Checks whether `T` is primitive, i.e., either an arithmetic type or
/// convertible to one of STL's string types.
template <class T>
struct is_primitive {
  static constexpr bool value = std::is_convertible_v<T, std::string>
                                || std::is_convertible_v<T, std::u16string>
                                || std::is_convertible_v<T, std::u32string>
                                || std::is_arithmetic_v<T>;
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
  static bool
  cmp_help_fun(const A*, const B*, bool*, std::integral_constant<bool, true>);

  template <class A, class B, class C>
  static void cmp_help_fun(const A*, const B*, void*, C);

  using result_type = decltype(cmp_help_fun(
    null_v<T1>, null_v<T2>, null_v<bool>,
    std::integral_constant<bool, std::is_arithmetic_v<T1>
                                   && std::is_arithmetic_v<T2>>{}));

public:
  static constexpr bool value = std::is_same_v<bool, result_type>;
};

/// Convenience alias for `is_comparable<T1, T2>::value`.
template <class T1, class T2>
inline constexpr bool is_comparable_v = is_comparable<T1, T2>::value;

/// Checks whether `T` behaves like a forward iterator.
template <class T>
class is_forward_iterator {
  template <class C>
  static bool sfinae(C& x, C& y,
                     // check for operator*
                     std::decay_t<decltype(*x)>* = nullptr,
                     // check for operator++ returning an iterator
                     std::decay_t<decltype(x = ++y)>* = nullptr,
                     // check for operator==
                     std::decay_t<decltype(x == y)>* = nullptr,
                     // check for operator!=
                     std::decay_t<decltype(x != y)>* = nullptr);

  static void sfinae(...);

  using result_type = decltype(sfinae(std::declval<T&>(), std::declval<T&>()));

public:
  static constexpr bool value = std::is_same_v<bool, result_type>;
};

/// Convenience alias for `is_forward_iterator<T>::value`.
template <class T>
inline constexpr bool is_forward_iterator_v = is_forward_iterator<T>::value;

/// Checks whether `T` has `begin()` and `end()` member
/// functions returning forward iterators.
template <class T>
class is_iterable {
  // this horrible code would just disappear if we had concepts
  template <class C>
  static bool sfinae(
    C* cc,
    // check if 'C::begin()' returns a forward iterator
    std::enable_if_t<is_forward_iterator_v<decltype(cc->begin())>>* = nullptr,
    // check if begin() and end() both exist and are comparable
    decltype(cc->begin() != cc->end())* = nullptr);

  // SFNINAE default
  static void sfinae(void*);

  using result_type = decltype(sfinae(null_v<std::decay_t<T>>));

public:
  static constexpr bool value = is_primitive<T>::value == false
                                && std::is_same_v<bool, result_type>;
};

template <class T>
inline constexpr bool is_iterable<T>::value;

template <class T>
inline constexpr bool is_iterable_v = is_iterable<T>::value;

/// Checks whether `T` is a non-const reference.
template <class T>
struct is_mutable_ref : std::false_type {};

template <class T>
struct is_mutable_ref<const T&> : std::false_type {};

template <class T>
struct is_mutable_ref<T&> : std::true_type {};

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
struct callable_trait<R(Ts...)> {
  /// The result type as returned by the function.
  using result_type = R;

  /// The unmodified argument types of the function.
  using arg_types = type_list<Ts...>;

  /// The argument types of the function without CV qualifiers.
  using decayed_arg_types = type_list<std::decay_t<Ts>...>;

  /// The signature of the function.
  using fun_sig = R(Ts...);

  /// The signature of the function, wrapped into a `std::function`.
  using fun_type = std::function<R(Ts...)>;

  /// Tells whether the function takes mutable references as argument.
  static constexpr bool mutates_args = (is_mutable_ref<Ts>::value || ...);

  /// Selects a suitable view type for passing a ::message to this function.
  using message_view_type
    = std::conditional_t<mutates_args, typed_message_view<std::decay_t<Ts>...>,
                         const_typed_message_view<std::decay_t<Ts>...>>;

  /// Tells the number of arguments of the function.
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
struct callable_trait<R (C::*)(Ts...) const> : callable_trait<R(Ts...)> {};

// member function pointer
template <class C, typename R, class... Ts>
struct callable_trait<R (C::*)(Ts...)> : callable_trait<R(Ts...)> {};

// good ol' noexcept function pointer
template <class R, class... Ts>
struct callable_trait<R (*)(Ts...) noexcept> : callable_trait<R(Ts...)> {};

// good ol' function pointer
template <class R, class... Ts>
struct callable_trait<R (*)(Ts...)> : callable_trait<R(Ts...)> {};

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
          bool IsFun = std::is_function_v<T>
                       || std::is_function_v<std::remove_pointer_t<T>>
                       || std::is_member_function_pointer_v<T>,
          bool HasApplyOp = has_apply_operator<T>::value>
struct get_callable_trait_helper {
  static constexpr bool valid = true;
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
  static constexpr bool valid = true;
  using type = callable_trait<decltype(&T::operator())>;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  using fun_sig = typename type::fun_sig;
  static constexpr size_t num_args = tl_size<arg_types>::value;
};

template <class T>
struct get_callable_trait_helper<T, false, false> {
  static constexpr bool valid = false;
};

/// Gets a callable trait for `T,` where `T` is a function object type,
/// i.e., a function, member function, or a class providing
/// the call operator.
template <class T>
struct get_callable_trait : get_callable_trait_helper<std::decay_t<T>> {};

template <class T>
using get_callable_trait_t = typename get_callable_trait<T>::type;

/// Checks whether `T` is a function, function object or member function.
template <class T>
struct is_callable {
  template <class C>
  static bool _fun(C*, get_callable_trait_t<C>* = nullptr);

  static void _fun(void*);

  using result_type = decltype(_fun(null_v<std::decay_t<T>>));

public:
  static constexpr bool value = std::is_same_v<bool, result_type>;
};

/// Convenience alias for `is_callable<T>::value`.
/// @relates is_callable
template <class T>
inline constexpr bool is_callable_v = is_callable<T>::value;

// Checks whether T has a member variable named `name`.
template <class T, bool IsScalar = std::is_scalar_v<T>>
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
  static constexpr bool value = sizeof(fun(null_v<derived>)) > 1;
};

template <class T>
class has_name<T, true> {
public:
  static constexpr bool value = false;
};

CAF_HAS_MEMBER_TRAIT(make_behavior);

/// Checks whether F is convertible to either `std::function<void (T&)>`
/// or `std::function<void (const T&)>`.
template <class F, class T>
struct is_handler_for {
  static constexpr bool value
    = std::is_convertible_v<F, std::function<void(T&)>>
      || std::is_convertible_v<F, std::function<void(const T&)>>;
};

/// Convenience alias for `is_handler_for<F, T>::value`.
template <class F, class T>
inline constexpr bool is_handler_for_v = is_handler_for<F, T>::value;

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

template <class F, class T>
using is_handler_for_ef = typename std::enable_if<is_handler_for_v<F, T>>::type;

// Checks whether T has a member function named `push_back` that takes an
// element of type `T::value_type`.
template <class T>
class has_push_back {
private:
  template <class U>
  static char fun(U*, decltype(std::declval<U&>().push_back(
                        std::declval<typename U::value_type>()))* = nullptr);

  static int fun(void*);

public:
  static constexpr bool value = sizeof(fun(null_v<T>)) == 1;
};

template <class T>
inline constexpr bool has_push_back_v = has_push_back<T>::value;

template <class T>
struct is_result : std::false_type {};

template <class T>
struct is_result<result<T>> : std::true_type {};

template <class T>
struct is_expected : std::false_type {};

template <class T>
struct is_expected<expected<T>> : std::true_type {};

template <class T>
inline constexpr bool is_expected_v = is_expected<T>::value;

/// Utility for fallbacks calling `static_assert`.
template <class>
struct always_false : std::false_type {};

template <class T>
inline constexpr bool always_false_v = always_false<T>::value;

/// Utility trait for checking whether T is a `std::pair`.
template <class T>
struct is_pair : std::false_type {};

template <class First, class Second>
struct is_pair<std::pair<First, Second>> : std::true_type {};

template <class T>
inline constexpr bool is_pair_v = is_pair<T>::value;

// -- traits to check for STL-style type aliases -------------------------------

CAF_HAS_ALIAS_TRAIT(value_type);

CAF_HAS_ALIAS_TRAIT(key_type);

CAF_HAS_ALIAS_TRAIT(mapped_type);

// -- constexpr functions for use in enable_if & friends -----------------------

/// Checks whether T behaves like `std::map`.
template <class T>
struct is_map_like {
  static constexpr bool value = is_iterable_v<T> && has_key_type_alias_v<T>
                                && has_mapped_type_alias_v<T>;
};

template <class T>
inline constexpr bool is_map_like_v = is_map_like<T>::value;

template <class T>
struct has_insert {
private:
  template <class List>
  static auto sfinae(List* l, typename List::value_type* x = nullptr)
    -> decltype(l->insert(l->end(), *x), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_insert_v = has_insert<T>::value;

template <class T>
struct has_size {
private:
  template <class List>
  static auto sfinae(List* l)
    -> decltype(static_cast<void>(l->size()), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_size_v = has_size<T>::value;

template <class T>
struct has_reserve {
private:
  template <class List>
  static auto sfinae(List* l) -> decltype(l->reserve(10), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_reserve_v = has_reserve<T>::value;

template <class T>
struct has_emplace_back {
private:
  template <class List>
  static auto sfinae(List* l)
    -> decltype(l->emplace_back(std::declval<typename List::value_type>()),
                std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_emplace_back_v = has_emplace_back<T>::value;

template <class T>
class has_call_error_handler {
private:
  template <class Actor>
  static auto sfinae(Actor* self)
    -> decltype(self->call_error_handler(std::declval<error&>()),
                std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_call_error_handler_v
  = has_call_error_handler<T>::value;

template <class T>
class has_add_awaited_response_handler {
private:
  template <class Actor>
  static auto sfinae(Actor* self)
    -> decltype(self->add_awaited_response_handler(std::declval<message_id>(),
                                                   std::declval<behavior&>()),
                std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_add_awaited_response_handler_v
  = has_add_awaited_response_handler<T>::value;

template <class T>
class has_add_multiplexed_response_handler {
private:
  template <class Actor>
  static auto sfinae(Actor* self)
    -> decltype(self->add_multiplexed_response_handler(
                  std::declval<message_id>(), std::declval<behavior&>()),
                std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

template <class T>
inline constexpr bool has_add_multiplexed_response_handler_v
  = has_add_multiplexed_response_handler<T>::value;

/// Checks whether T behaves like `std::vector`, `std::list`, or `std::set`.
template <class T>
struct is_list_like {
  static constexpr bool value = is_iterable<T>::value
                                && has_value_type_alias_v<T>
                                && !has_mapped_type_alias_v<T>
                                && has_insert_v<T> && has_size_v<T>;
};

template <class T>
inline constexpr bool is_list_like_v = is_list_like<T>::value;

template <class T, class To>
class has_convertible_data_member {
private:
  template <class U>
  static auto sfinae(U* x)
    -> std::integral_constant<bool,
                              std::is_convertible_v<decltype(x->data()), To*>>;

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

/// Convenience alias for `has_convertible_data_member<T, To>::value`.
template <class T, class To>
inline constexpr bool has_convertible_data_member_v
  = has_convertible_data_member<T, To>::value;

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
inline constexpr bool is_stl_tuple_type_v = is_stl_tuple_type<T>::value;

template <class Inspector>
class has_context {
private:
  template <class F>
  static auto sfinae(F& f) -> decltype(f.context());

  static void sfinae(...);

  using result_type = decltype(sfinae(std::declval<Inspector&>()));

public:
  static constexpr bool value = std::is_same_v<result_type, execution_unit*>;
};

/// Checks whether `T` provides an `inspect` overload for `Inspector`.
template <class Inspector, class T>
class has_inspect_overload {
private:
  template <class U>
  static auto sfinae(Inspector& x, U& y)
    -> decltype(inspect(x, y), std::true_type{});

  static std::false_type sfinae(Inspector&, ...);

  using result_type
    = decltype(sfinae(std::declval<Inspector&>(), std::declval<T&>()));

public:
  static constexpr bool value = result_type::value;
};

/// Convenience alias for `has_inspect_overload<Inspector, T>::value`.
template <class Inspector, class T>
inline constexpr bool has_inspect_overload_v
  = has_inspect_overload<Inspector, T>::value;

/// Checks whether the inspector has a `builtin_inspect` overload for `T`.
template <class Inspector, class T>
class has_builtin_inspect {
private:
  template <class I, class U>
  static auto sfinae(I& f, U& x)
    -> decltype(f.builtin_inspect(x), std::true_type{});

  template <class I>
  static std::false_type sfinae(I&, ...);

  using sfinae_result
    = decltype(sfinae(std::declval<Inspector&>(), std::declval<T&>()));

public:
  static constexpr bool value = sfinae_result::value;
};

/// Convenience alias for `has_builtin_inspect<Inspector, T>::value`.
template <class Inspector, class T>
inline constexpr bool has_builtin_inspect_v
  = has_builtin_inspect<Inspector, T>::value;

/// Checks whether the inspector has an `opaque_value` overload for `T`.
template <class Inspector, class T>
class accepts_opaque_value {
private:
  template <class F, class U>
  static auto sfinae(F* f, U* x)
    -> decltype(f->opaque_value(*x), std::true_type{});

  static std::false_type sfinae(...);

  using sfinae_result = decltype(sfinae(null_v<Inspector>, null_v<T>));

public:
  static constexpr bool value = sfinae_result::value;
};

/// Convenience alias for `accepts_opaque_value<Inspector, T>::value`.
template <class Inspector, class T>
inline constexpr bool accepts_opaque_value_v
  = accepts_opaque_value<Inspector, T>::value;

/// Checks whether `T` is primitive, i.e., either an arithmetic type or
/// convertible to one of STL's string types.
template <class T, bool IsLoading>
struct is_builtin_inspector_type {
  static constexpr bool value = std::is_arithmetic_v<T>;
};

template <bool IsLoading>
struct is_builtin_inspector_type<std::byte, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type<span<std::byte>, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type<std::string, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type<std::u16string, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type<std::u32string, IsLoading> {
  static constexpr bool value = true;
};

template <>
struct is_builtin_inspector_type<std::string_view, false> {
  static constexpr bool value = true;
};

template <>
struct is_builtin_inspector_type<span<const std::byte>, false> {
  static constexpr bool value = true;
};

/// Checks whether `T` is a `std::optional`.
template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
inline constexpr bool is_optional_v = is_optional<T>::value;

template <class T>
struct unboxed_oracle {
  using type = T;
};

template <class T>
struct unboxed_oracle<std::optional<T>> {
  using type = T;
};

template <class T>
struct unboxed_oracle<expected<T>> {
  using type = T;
};

template <class T>
using unboxed_t = typename unboxed_oracle<T>::type;

/// Evaluates to true if `T` is a std::string or is convertible to a `const
/// char*`.
template <class T>
inline constexpr bool is_string_or_cstring_v
  = std::is_convertible_v<T, const char*>
    || std::is_same_v<std::string, std::decay_t<T>>;

template <class T>
inline constexpr bool is_64bit_integer_v = std::is_same_v<T, int64_t>
                                           || std::is_same_v<T, uint64_t>;

/// Checks whether `T` has a static member function called `init_host_system`.
template <class T>
struct has_init_host_system {
// GNU g++ 8.5.0 (almalinux-8) has a known bug where [[nodiscard] values are
// reported as warnings, even in unevaluated context.
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=89070
#if !defined(__clang__) && defined(__GNUC__)
  CAF_PUSH_UNUSED_RESULT_WARNING
#endif
  template <class U>
  static auto sfinae(U*) -> decltype(U::init_host_system(), std::true_type());
#if !defined(__clang__) && defined(__GNUC__)
  CAF_POP_WARNINGS
#endif

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using type = decltype(sfinae<T>(nullptr));
  static constexpr bool value = type::value;
};

/// Convenience alias for `has_init_host_system<T>::value`.
template <class T>
inline constexpr bool has_init_host_system_v = has_init_host_system<T>::value;

/// Drop-in replacement for C++23's std::to_underlying.
template <class Enum>
[[nodiscard]] constexpr auto to_underlying(Enum e) noexcept {
  return static_cast<std::underlying_type_t<Enum>>(e);
}

} // namespace caf::detail

#undef CAF_HAS_MEMBER_TRAIT
#undef CAF_HAS_ALIAS_TRAIT
