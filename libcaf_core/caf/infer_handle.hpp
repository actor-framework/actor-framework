// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/typed_behavior.hpp"

namespace caf {

template <class... Sigs>
class typed_event_based_actor;

namespace io {

template <class... Sigs>
class typed_broker;

} // namespace io

enum class spawn_mode { function, function_with_selfptr, clazz };

template <spawn_mode X>
using spawn_mode_token = std::integral_constant<spawn_mode, X>;

// default: dynamically typed actor without self pointer
template <class Result, class FirstArg,
          bool FirstArgValid
          = std::is_base_of_v<local_actor, std::remove_pointer_t<FirstArg>>>
struct infer_handle_from_fun_impl {
  using type = actor;
  using impl = event_based_actor;
  using behavior_type = behavior;
  static constexpr spawn_mode mode = spawn_mode::function;
};

// dynamically typed actor returning a behavior
template <class Impl>
struct infer_handle_from_fun_impl<void, Impl*, true> {
  using type = actor;
  using impl = Impl;
  using behavior_type = behavior;
  static constexpr spawn_mode mode = spawn_mode::function_with_selfptr;
};

// dynamically typed actor with self pointer
template <class Impl>
struct infer_handle_from_fun_impl<behavior, Impl*, true> {
  using type = actor;
  using impl = Impl;
  using behavior_type = behavior;
  static constexpr spawn_mode mode = spawn_mode::function_with_selfptr;
};

// statically typed actor returning a behavior
template <class... Sigs, class Impl>
struct infer_handle_from_fun_impl<typed_behavior<Sigs...>, Impl, false> {
  using type = typed_actor<Sigs...>;
  using impl = typed_event_based_actor<Sigs...>;
  using behavior_type = typed_behavior<Sigs...>;
  static constexpr spawn_mode mode = spawn_mode::function;
};

// statically typed actor with self pointer
template <class... Sigs, class Impl>
struct infer_handle_from_fun_impl<typed_behavior<Sigs...>, Impl*, true> {
  static_assert(std::is_base_of_v<typed_event_based_actor<Sigs...>, Impl>
                  || std::is_base_of_v<io::typed_broker<Sigs...>, Impl>,
                "Self pointer does not match the returned behavior type.");
  using type = typed_actor<Sigs...>;
  using impl = Impl;
  using behavior_type = typed_behavior<Sigs...>;
  static constexpr spawn_mode mode = spawn_mode::function_with_selfptr;
};

/// Deduces an actor handle type from a function or function object.
template <class F, class Trait = detail::get_callable_trait_t<F>>
struct infer_handle_from_fun {
  using result_type = typename Trait::result_type;
  using arg_types = typename Trait::arg_types;
  using first_arg = detail::tl_head_t<arg_types>;
  using delegate = infer_handle_from_fun_impl<result_type, first_arg>;
  using type = typename delegate::type;
  using impl = typename delegate::impl;
  using behavior_type = typename delegate::behavior_type;
  using fun_type = typename Trait::fun_type;
  static constexpr spawn_mode mode = delegate::mode;
};

/// @relates infer_handle_from_fun
template <class F>
using infer_handle_from_fun_t = typename infer_handle_from_fun<F>::type;

/// @relates infer_handle_from_fun
template <class T>
using infer_impl_from_fun_t = typename infer_handle_from_fun<T>::impl;

template <class T>
struct infer_handle_from_behavior {
  using type = actor;
};

template <class... Sigs>
struct infer_handle_from_behavior<typed_behavior<Sigs...>> {
  using type = typed_actor<Sigs...>;
};

/// Deduces `actor` for dynamically typed actors, otherwise `typed_actor<...>`
/// is deduced.
template <class T, bool = std::is_base_of_v<abstract_actor, T>>
struct infer_handle_from_class {
  using type =
    typename infer_handle_from_behavior<typename T::behavior_type>::type;
  static constexpr spawn_mode mode = spawn_mode::clazz;
};

template <class T>
struct infer_handle_from_class<T, false> {
  // nop; this enables SFINAE
};

/// @relates infer_handle_from_class
template <class T>
using infer_handle_from_class_t = typename infer_handle_from_class<T>::type;

template <class T>
struct is_handle : std::false_type {};

template <>
struct is_handle<actor> : std::true_type {};

template <>
struct is_handle<strong_actor_ptr> : std::true_type {};

template <class... Ts>
struct is_handle<typed_actor<Ts...>> : std::true_type {};

/// Convenience alias for `is_handle<T>::value`.
template <class T>
inline constexpr bool is_handle_v = is_handle<T>::value;

} // namespace caf
