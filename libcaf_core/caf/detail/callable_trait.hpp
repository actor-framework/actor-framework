// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/fwd.hpp"

#include <functional>
#include <type_traits>

namespace caf::detail {

/// Checks whether `T` is a non-const reference.
template <class T>
struct is_mutable_ref_oracle : std::false_type {};

template <class T>
struct is_mutable_ref_oracle<const T&> : std::false_type {};

template <class T>
struct is_mutable_ref_oracle<T&> : std::true_type {};

template <class T>
static constexpr bool is_mutable_ref = is_mutable_ref_oracle<T>::value;

/// Defines `result_type,` `arg_types,` and `fun_type`. Functor is
///    (a) a member function pointer, (b) a function,
///    (c) a function pointer, (d) an std::function.
///
/// `result_type` is the result type found in the signature.
/// `arg_types` are the argument types as @ref type_list.
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
  static constexpr bool mutates_args = (is_mutable_ref<Ts> || ...);

  /// A view type for passing a ::message to this function with mutable access.
  using mutable_message_view_type = typed_message_view<std::decay_t<Ts>...>;

  /// Selects a suitable view type for passing a ::message to this function.
  using message_view_type
    = std::conditional_t<mutates_args, mutable_message_view_type,
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
concept has_apply_operator = requires { &T::operator(); };

template <class T>
struct get_callable_trait_helper {
  static constexpr bool valid = false;
};

template <class T>
concept function_or_member_function
  = std::is_function_v<T> || std::is_function_v<std::remove_pointer_t<T>>
    || std::is_member_function_pointer_v<T>;

template <function_or_member_function T>
struct get_callable_trait_helper<T> {
  static constexpr bool valid = true;
  using type = callable_trait<T>;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  using fun_sig = typename type::fun_sig;
  static constexpr size_t num_args = type::num_args;
};

template <has_apply_operator T>
struct get_callable_trait_helper<T> {
  static constexpr bool valid = true;
  using type = callable_trait<decltype(&T::operator())>;
  using result_type = typename type::result_type;
  using arg_types = typename type::arg_types;
  using fun_type = typename type::fun_type;
  using fun_sig = typename type::fun_sig;
  static constexpr size_t num_args = type::num_args;
};

template <class T>
using get_callable_trait =
  typename get_callable_trait_helper<std::decay_t<T>>::type;

/// Checks whether `T` is a function, function object or member function.
template <class T>
concept callable
  = requires { typename get_callable_trait_helper<std::decay_t<T>>::type; };

} // namespace caf::detail
