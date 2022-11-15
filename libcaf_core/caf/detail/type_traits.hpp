// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "caf/detail/is_complete.hpp"
#include "caf/detail/is_one_of.hpp"
#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"
#include "caf/timestamp.hpp"

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

template <class T>
constexpr T* null_v = nullptr;

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

template <class T>
constexpr bool is_iterable_v = is_iterable<T>::value;

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
  /// The result type as returned by the function.
  using result_type = R;

  /// The unmodified argument types of the function.
  using arg_types = type_list<Ts...>;

  /// The argument types of the function without CV qualifiers.
  using decayed_arg_types = type_list<std::decay_t<Ts>...>;

  /// The signature of the function.
  using fun_sig = R (Ts...);

  /// The signature of the function, wrapped into a `std::function`.
  using fun_type = std::function<R (Ts...)>;

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

CAF_HAS_MEMBER_TRAIT(clear);
CAF_HAS_MEMBER_TRAIT(data);
CAF_HAS_MEMBER_TRAIT(make_behavior);
CAF_HAS_MEMBER_TRAIT(size);

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

template <class T>
constexpr bool always_false_v = always_false<T>::value;

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
struct has_size {
private:
  template <class List>
  static auto sfinae(List* l) -> decltype(l->size(), std::true_type());

  template <class U>
  static auto sfinae(...) -> std::false_type;

  using sfinae_type = decltype(sfinae<T>(nullptr));

public:
  static constexpr bool value = sfinae_type::value;
};

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
constexpr bool has_reserve_v = has_reserve<T>::value;

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
constexpr bool has_emplace_back_v = has_emplace_back<T>::value;

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
constexpr bool has_call_error_handler_v = has_call_error_handler<T>::value;

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
constexpr bool has_add_awaited_response_handler_v
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
constexpr bool has_add_multiplexed_response_handler_v
  = has_add_multiplexed_response_handler<T>::value;

/// Checks whether T behaves like `std::vector`, `std::list`, or `std::set`.
template <class T>
struct is_list_like {
  static constexpr bool value = is_iterable<T>::value
                                && has_value_type_alias<T>::value
                                && !has_mapped_type_alias<T>::value
                                && has_insert<T>::value && has_size<T>::value;
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
    -> decltype(CAF_IGNORE_UNUSED(x->apply(std::declval<Arg>())),
                std::true_type{});

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

template <class Inspector>
class has_context {
private:
  template <class F>
  static auto sfinae(F& f) -> decltype(f.context());

  static void sfinae(...);

  using result_type = decltype(sfinae(std::declval<Inspector&>()));

public:
  static constexpr bool value
    = std::is_same<result_type, execution_unit*>::value;
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

/// Checks whether inspectors are required to provide a `value` overload for T.
template <bool IsLoading, class T>
struct is_trivial_inspector_value;

template <class T>
struct is_trivial_inspector_value<true, T> {
  static constexpr bool value = false;
};

template <class T>
struct is_trivial_inspector_value<false, T> {
  static constexpr bool value = std::is_convertible<T, string_view>::value;
};

#define CAF_ADD_TRIVIAL_LOAD_INSPECTOR_VALUE(type)                             \
  template <>                                                                  \
  struct is_trivial_inspector_value<true, type> {                              \
    static constexpr bool value = true;                                        \
  };

#define CAF_ADD_TRIVIAL_SAVE_INSPECTOR_VALUE(type)                             \
  template <>                                                                  \
  struct is_trivial_inspector_value<false, type> {                             \
    static constexpr bool value = true;                                        \
  };

#define CAF_ADD_TRIVIAL_INSPECTOR_VALUE(type)                                  \
  CAF_ADD_TRIVIAL_LOAD_INSPECTOR_VALUE(type)                                   \
  CAF_ADD_TRIVIAL_SAVE_INSPECTOR_VALUE(type)

CAF_ADD_TRIVIAL_INSPECTOR_VALUE(bool)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(float)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(double)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(long double)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(std::u16string)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(std::u32string)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(std::vector<bool>)
CAF_ADD_TRIVIAL_INSPECTOR_VALUE(span<byte>)

CAF_ADD_TRIVIAL_SAVE_INSPECTOR_VALUE(span<const byte>)

CAF_ADD_TRIVIAL_LOAD_INSPECTOR_VALUE(std::string)

#undef CAF_ADD_TRIVIAL_INSPECTOR_VALUE
#undef CAF_ADD_TRIVIAL_SAVE_INSPECTOR_VALUE
#undef CAF_ADD_TRIVIAL_LOAD_INSPECTOR_VALUE

template <bool IsLoading, class T>
constexpr bool is_trivial_inspector_value_v
  = is_trivial_inspector_value<IsLoading, T>::value;

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

/// Checks whether `T` is primitive, i.e., either an arithmetic type or
/// convertible to one of STL's string types.
template <class T, bool IsLoading>
struct is_builtin_inspector_type {
  static constexpr bool value = std::is_arithmetic<T>::value;
};

template <bool IsLoading>
struct is_builtin_inspector_type<byte, IsLoading> {
  static constexpr bool value = true;
};

template <bool IsLoading>
struct is_builtin_inspector_type<span<byte>, IsLoading> {
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
struct is_builtin_inspector_type<string_view, false> {
  static constexpr bool value = true;
};

template <>
struct is_builtin_inspector_type<span<const byte>, false> {
  static constexpr bool value = true;
};

/// Checks whether `T` is a `std::optional`.
template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {};

template <class T>
constexpr bool is_optional_v = is_optional<T>::value;

template <class T>
struct unboxed_oracle {
  using type = T;
};

template <class T>
struct unboxed_oracle<std::optional<T>> {
  using type = T;
};

template <class T>
using unboxed_t = typename unboxed_oracle<T>::type;

} // namespace caf::detail

#undef CAF_HAS_MEMBER_TRAIT
#undef CAF_HAS_ALIAS_TRAIT
