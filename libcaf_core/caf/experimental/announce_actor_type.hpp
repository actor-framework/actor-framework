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

#ifndef CAF_EXPERIMENTAL_ANNOUNCE_ACTOR_HPP
#define CAF_EXPERIMENTAL_ANNOUNCE_ACTOR_HPP

#include <type_traits>

#include "caf/actor.hpp"
#include "caf/actor_addr.hpp"
#include "caf/typed_behavior.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/detail/singletons.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/actor_registry.hpp"

namespace caf {
namespace experimental {

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
template <class... Sigs, class FirstArg>
struct infer_handle_from_fun_impl<typed_behavior<Sigs...>, FirstArg, false> {
  using type = typed_actor<Sigs...>;
  using impl = typed_event_based_actor<Sigs...>;
  using behavior_type = typed_behavior<Sigs...>;
  static constexpr spawn_mode mode = spawn_mode::function;
};

// statically typed actor with self pointer
template <class Result, class... Sigs>
struct infer_handle_from_fun_impl<Result, typed_event_based_actor<Sigs...>*, true> {
  using type = typed_actor<Sigs...>;
  using impl = typed_event_based_actor<Sigs...>;
  using behavior_type = typed_behavior<Sigs...>;
  static constexpr spawn_mode mode = spawn_mode::function_with_selfptr;
};

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

template <class T>
struct infer_handle_from_behavior {
  using type = actor;
};

template <class... Sigs>
struct infer_handle_from_behavior<typed_behavior<Sigs...>> {
  using type = typed_actor<Sigs...>;
};

using spawn_result = std::pair<actor_addr, std::set<std::string>>;

using spawn_fun = std::function<spawn_result (message)>;

template <class Trait, class F>
spawn_result dyn_spawn_impl(F ibf) {
  using impl = typename Trait::impl;
  using behavior_t = typename Trait::behavior_type;
  auto ptr = make_counted<impl>();
  ptr->initial_behavior_fac([=](local_actor* self) -> behavior {
    auto res = ibf(self);
    if (res && res->size() > 0 && res->template match_element<behavior_t>(0)) {
      return std::move(res->template get_as_mutable<behavior_t>(0).unbox());
    }
    return {};
  });
  ptr->launch(nullptr, false, false);
  return {ptr->address(), Trait::type::message_types()};
}

template <class Trait, class F>
spawn_result dyn_spawn(F fun, message& msg,
                       spawn_mode_token<spawn_mode::function>) {
  return dyn_spawn_impl<Trait>([=](local_actor*) -> optional<message> {
    return const_cast<message&>(msg).apply(fun);
  });
}

template <class Trait, class F>
spawn_result dyn_spawn(F fun, message& msg,
                       spawn_mode_token<spawn_mode::function_with_selfptr>) {
  return dyn_spawn_impl<Trait>([=](local_actor* self) -> optional<message> {
    // we can't use make_message here because of the implicit conversions
    using storage = detail::tuple_vals<typename Trait::impl*>;
    auto ptr = make_counted<storage>(static_cast<typename Trait::impl*>(self));
    auto m = message{detail::message_data::cow_ptr{std::move(ptr)}} + msg;
    return m.apply(fun);
  });
}

template <class F>
spawn_fun make_spawn_fun(F fun) {
  return [fun](message msg) -> spawn_result {
    using trait = infer_handle_from_fun<F>;
    spawn_mode_token<trait::mode> tk;
    return dyn_spawn<trait>(fun, msg, tk);
  };
}

template <class T, class... Ts>
spawn_fun make_spawn_fun() {
  static_assert(std::is_same<T*, decltype(new T(std::declval<Ts>()...))>::value,
                "no constructor for T(Ts...) exists");
  static_assert(detail::conjunction<
                  std::is_lvalue_reference<Ts>::value...
                >::value,
                "all Ts must be lvalue references");
  static_assert(std::is_base_of<local_actor, T>::value,
                "T is not derived from local_actor");
  using handle =
    typename infer_handle_from_behavior<
      typename T::behavior_type
    >::type;
  using pointer = intrusive_ptr<T>;
  auto factory = &make_counted<T, Ts...>;
  return [=](message msg) -> spawn_result {
    pointer ptr;
    auto res = msg.apply(factory);
    if (! res || res->empty() || ! res->template match_element<pointer>(0))
      return {};
    ptr = std::move(res->template get_as_mutable<pointer>(0));
    ptr->launch(nullptr, false, false);
    return {ptr->address(), handle::message_types()};
  };
}

actor spawn_announce_actor_type_server();

void announce_actor_type_impl(std::string&& name, spawn_fun f);

template <class F>
void announce_actor_type(std::string name, F fun) {
  announce_actor_type_impl(std::move(name), make_spawn_fun(fun));
}

template <class T, class... Ts>
void announce_actor_type(std::string name) {
  announce_actor_type_impl(std::move(name), make_spawn_fun<T, Ts...>());
}

} // namespace experimental
} // namespace caf

#endif // CAF_EXPERIMENTAL_ANNOUNCE_ACTOR_HPP
