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

#ifndef CAF_INFER_HANDLE_HPP
#define CAF_INFER_HANDLE_HPP

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/abstract_composable_behavior.hpp"

namespace caf {

template <class... Sigs>
class typed_event_based_actor;

namespace io {

template <class... Sigs>
class typed_broker;

} // namespace io

enum class spawn_mode {
  function,
  function_with_selfptr,
  clazz
};

template <spawn_mode X>
using spawn_mode_token = std::integral_constant<spawn_mode, X>;

// default: dynamically typed actor without self pointer
template <class Result,
          class FirstArg,
          bool FirstArgValid =
            std::is_base_of<
              local_actor,
              typename std::remove_pointer<FirstArg>::type
            >::value>
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
  static_assert(std::is_base_of<typed_event_based_actor<Sigs...>, Impl>::value
                || std::is_base_of<io::typed_broker<Sigs...>, Impl>::value,
                "Self pointer does not match the returned behavior type.");
  using type = typed_actor<Sigs...>;
  using impl = Impl;
  using behavior_type = typed_behavior<Sigs...>;
  static constexpr spawn_mode mode = spawn_mode::function_with_selfptr;
};

/// Deduces an actor handle type from a function or function object.
template <class F, class Trait = typename detail::get_callable_trait<F>::type>
struct infer_handle_from_fun {
  using result_type = typename Trait::result_type;
  using arg_types = typename Trait::arg_types;
  using first_arg = typename detail::tl_head<arg_types>::type;
  using delegate = infer_handle_from_fun_impl<result_type, first_arg>;
  using type = typename delegate::type;
  using impl = typename delegate::impl;
  using behavior_type = typename delegate::behavior_type;
  using fun_type = typename Trait::fun_type;
  static constexpr spawn_mode mode = delegate::mode;
};

/// @relates infer_handle_from_fun
template <class T>
using infer_handle_from_fun_t = typename infer_handle_from_fun<T>::type;

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
template <class T, bool = std::is_base_of<abstract_actor, T>::value>
struct infer_handle_from_class {
  using type =
    typename infer_handle_from_behavior<
      typename T::behavior_type
    >::type;
  static constexpr spawn_mode mode = spawn_mode::clazz;
};

template <class T>
struct infer_handle_from_class<T, false> {
  // nop; this enables SFINAE for spawn to differentiate between
  // spawns using actor classes or composable states
};

/// @relates infer_handle_from_class
template <class T>
using infer_handle_from_class_t = typename infer_handle_from_class<T>::type;

template <class T, bool = std::is_base_of<abstract_composable_behavior, T>::value>
struct infer_handle_from_state {
  using type = typename T::handle_type;
};

template <class T>
struct infer_handle_from_state<T, false> {
  // nop; this enables SFINAE for spawn to differentiate between
  // spawns using actor classes or composable states
};

/// @relates infer_handle_from_state
template <class T>
using infer_handle_from_state_t = typename infer_handle_from_state<T>::type;

template <class T>
struct is_handle : std::false_type {};

template <>
struct is_handle<actor> : std::true_type {};

template <>
struct is_handle<strong_actor_ptr> : std::true_type {};

template <class... Ts>
struct is_handle<typed_actor<Ts...>> : std::true_type {};

} // namespace caf

#endif // CAF_INFER_HANDLE_HPP
