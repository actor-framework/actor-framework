/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2014                                                  *
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

#ifndef CAF_SPAWN_HPP
#define CAF_SPAWN_HPP

#include <type_traits>

#include "caf/spawn_fwd.hpp"
#include "caf/typed_actor.hpp"
#include "caf/spawn_options.hpp"
#include "caf/typed_event_based_actor.hpp"

#include "caf/policy/no_resume.hpp"
#include "caf/policy/prioritizing.hpp"
#include "caf/policy/no_scheduling.hpp"
#include "caf/policy/actor_policies.hpp"
#include "caf/policy/nestable_invoke.hpp"
#include "caf/policy/not_prioritizing.hpp"
#include "caf/policy/sequential_invoke.hpp"
#include "caf/policy/event_based_resume.hpp"
#include "caf/policy/cooperative_scheduling.hpp"

#include "caf/detail/logging.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/detail/make_counted.hpp"
#include "caf/detail/proper_actor.hpp"
#include "caf/detail/typed_actor_util.hpp"
#include "caf/detail/implicit_conversions.hpp"

namespace caf {

class execution_unit;

/**
 * Marker interface that prevents `spawn` from wrapping the a class
 * in a `proper_actor` when spawning new instances.
 */
class spawn_as_is {};

/**
 * Returns a newly spawned instance of type `C` using `args...` as constructor
 * arguments. The instance will be added to the job list of `host`. However,
 * before the instance is launched, `before_launch_fun` will be called, e.g.,
 * to join a group before the actor is running.
 */
template <class C, spawn_options Os, class BeforeLaunch, class... Ts>
intrusive_ptr<C> spawn_impl(execution_unit* host,
                            BeforeLaunch before_launch_fun, Ts&&... args) {
  static_assert(!std::is_base_of<blocking_actor, C>::value
                || has_blocking_api_flag(Os),
                "C is derived from blocking_actor but "
                "spawned without blocking_api_flag");
  static_assert(is_unbound(Os),
                "top-level spawns cannot have monitor or link flag");
  CAF_LOGF_TRACE("spawn " << detail::demangle<C>());
  using scheduling_policy =
    typename std::conditional<
      has_detach_flag(Os) || has_blocking_api_flag(Os),
      policy::no_scheduling,
      policy::cooperative_scheduling
    >::type;
  using priority_policy =
    typename std::conditional<
      has_priority_aware_flag(Os),
      policy::prioritizing,
      policy::not_prioritizing
    >::type;
  using resume_policy =
    typename std::conditional<
      has_blocking_api_flag(Os),
      policy::no_resume,
      policy::event_based_resume
    >::type;
  using invoke_policy =
    typename std::conditional<
      has_blocking_api_flag(Os),
      policy::nestable_invoke,
      policy::sequential_invoke
    >::type;
  using policy_token =
    policy::actor_policies<
      scheduling_policy,
      priority_policy,
      resume_policy,
      invoke_policy
    >;
  using actor_impl =
    typename std::conditional<
      std::is_base_of<spawn_as_is, C>::value,
      C,
      detail::proper_actor<C, policy_token>
    >::type;
  auto ptr = detail::make_counted<actor_impl>(std::forward<Ts>(args)...);
  // actors start with a reference count of 1, hence we need to deref ptr once
  CAF_REQUIRE(!ptr->unique());
  ptr->deref();
  CAF_LOGF_DEBUG("spawned actor with ID " << ptr->id());
  CAF_PUSH_AID(ptr->id());
  before_launch_fun(ptr.get());
  ptr->launch(has_hide_flag(Os), host);
  return ptr;
}

/**
 * Converts `scoped_actor` and pointers to actors to handles of type `actor`
 * but simply forwards any other argument in the same way `std::forward` does.
 */
template <class T>
typename std::conditional<
  is_convertible_to_actor<typename std::decay<T>::type>::value,
  actor,
  T&&
>::type
spawn_fwd(typename std::remove_reference<T>::type& arg) noexcept {
  return static_cast<T&&>(arg);
}

/**
 * Converts `scoped_actor` and pointers to actors to handles of type `actor`
 * but simply forwards any other argument in the same way `std::forward` does.
 */
template <class T>
typename std::conditional<
  is_convertible_to_actor<typename std::decay<T>::type>::value,
  actor,
  T&&
>::type
spawn_fwd(typename std::remove_reference<T>::type&& arg) noexcept {
  static_assert(!std::is_lvalue_reference<T>::value,
                "silently converting an lvalue to an rvalue");
  return static_cast<T&&>(arg);
}

/**
 * Called by `spawn` when used to create a class-based actor (usually
 * should not be called by users of the library). This function
 * simply forwards its arguments to `spawn_impl` using `spawn_fwd`.
 */
template <class C, spawn_options Os, typename BeforeLaunch, class... Ts>
intrusive_ptr<C> spawn_class(execution_unit* host,
                             BeforeLaunch before_launch_fun, Ts&&... args) {
  return spawn_impl<C, Os>(host, before_launch_fun,
                           spawn_fwd<Ts>(args)...);
}

/**
 * Called by `spawn` when used to create a functor-based actor (usually
 * should not be called by users of the library). This function
 * selects a proper implementation class and then delegates to `spawn_class`.
 */
template <spawn_options Os, typename BeforeLaunch, typename F, class... Ts>
actor spawn_functor(execution_unit* eu, BeforeLaunch cb, F fun, Ts&&... args) {
  using trait = typename detail::get_callable_trait<F>::type;
  using arg_types = typename trait::arg_types;
  using first_arg = typename detail::tl_head<arg_types>::type;
  using base_class =
    typename std::conditional<
      std::is_pointer<first_arg>::value,
      typename std::remove_pointer<first_arg>::type,
      typename std::conditional<
        has_blocking_api_flag(Os),
        blocking_actor,
        event_based_actor
      >::type
    >::type;
  constexpr bool has_blocking_base =
    std::is_base_of<blocking_actor, base_class>::value;
  static_assert(has_blocking_base || !has_blocking_api_flag(Os),
                "blocking functor-based actors "
                "need to be spawned using the blocking_api flag");
  static_assert(!has_blocking_base || has_blocking_api_flag(Os),
                "non-blocking functor-based actors "
                "cannot be spawned using the blocking_api flag");
  using impl_class = typename base_class::functor_based;
  return spawn_class<impl_class, Os>(eu, cb, fun, std::forward<Ts>(args)...);
}

/**
 * @ingroup ActorCreation
 * @{
 */

/**
 * Returns a new actor of type `C` using `args...` as constructor
 * arguments. The behavior of `spawn` can be modified by setting `Os`, e.g.,
 * to opt-out of the cooperative scheduling.
 */
template <class C, spawn_options Os = no_spawn_options, class... Ts>
actor spawn(Ts&&... args) {
  return spawn_class<C, Os>(nullptr, empty_before_launch_callback{},
                            std::forward<Ts>(args)...);
}

/**
 * Returns a new functor-based actor. The first argument must be the functor,
 * the remainder of `args...` is used to invoke the functor.
 * The behavior of `spawn` can be modified by setting `Os`, e.g.,
 * to opt-out of the cooperative scheduling.
 */
template <spawn_options Os = no_spawn_options, class... Ts>
actor spawn(Ts&&... args) {
  static_assert(sizeof...(Ts) > 0, "too few arguments provided");
  return spawn_functor<Os>(nullptr, empty_before_launch_callback{},
                           std::forward<Ts>(args)...);
}

/**
 * Returns a new actor that immediately, i.e., before this function
 * returns, joins `grp` of type `C` using `args` as constructor arguments
 */
template <class C, spawn_options Os = no_spawn_options, class... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
  return spawn_class<C, Os>(nullptr, group_subscriber{grp},
                 std::forward<Ts>(args)...);
}

/**
 * Returns a new actor that immediately, i.e., before this function
 * returns, joins `grp`. The first element of `args` must
 * be the functor, the remaining arguments its arguments.
 */
template <spawn_options Os = no_spawn_options, class... Ts>
actor spawn_in_group(const group& grp, Ts&&... args) {
  static_assert(sizeof...(Ts) > 0, "too few arguments provided");
  return spawn_functor<Os>(nullptr, group_subscriber{grp},
               std::forward<Ts>(args)...);
}

/**
 * Base class for strongly typed actors using a functor-based implementation.
 */
template <class... Rs>
class functor_based_typed_actor : public typed_event_based_actor<Rs...> {
 public:
  /**
   * Base class for actors using given interface.
   */
  using base = typed_event_based_actor<Rs...>;

  /**
   * Pointer to the base class.
   */
  using pointer = base*;

  /**
   * Behavior with proper type information.
   */
  using behavior_type = typename base::behavior_type;

  /**
   * First valid functor signature.
   */
  using no_arg_fun = std::function<behavior_type()>;

  /**
   * Second valid functor signature.
   */
  using one_arg_fun1 = std::function<behavior_type(pointer)>;

  /**
   * Third (and last) valid functor signature.
   */
  using one_arg_fun2 = std::function<void(pointer)>;

  /**
   * Creates a new instance from given functor, binding `args...`
   * to the functor.
   */
  template <class F, class... Ts>
  functor_based_typed_actor(F fun, Ts&&... args) {
    using trait = typename detail::get_callable_trait<F>::type;
    using arg_types = typename trait::arg_types;
    using result_type = typename trait::result_type;
    constexpr bool returns_behavior =
      std::is_same<result_type, behavior_type>::value;
    constexpr bool uses_first_arg = std::is_same<
      typename detail::tl_head<arg_types>::type, pointer>::value;
    std::integral_constant<bool, returns_behavior> token1;
    std::integral_constant<bool, uses_first_arg> token2;
    set(token1, token2, std::move(fun), std::forward<Ts>(args)...);
  }

 protected:
  behavior_type make_behavior() override {
    return m_fun(this);
  }

 private:
  template <class F>
  void set(std::true_type, std::true_type, F&& fun) {
    // behavior_type (pointer)
    m_fun = std::forward<F>(fun);
  }

  template <class F>
  void set(std::false_type, std::true_type, F fun) {
    // void (pointer)
    m_fun = [fun](pointer ptr) {
      fun(ptr);
      return behavior_type{};

    };
  }

  template <class F>
  void set(std::true_type, std::false_type, F fun) {
    // behavior_type ()
    m_fun = [fun](pointer) { return fun(); };
  }

  // (false_type, false_type) is an invalid functor for typed actors

  template <class Token, typename F, typename T0, class... Ts>
  void set(Token t1, std::true_type t2, F fun, T0&& arg0, Ts&&... args) {
    set(t1, t2,
      std::bind(fun, std::placeholders::_1, std::forward<T0>(arg0),
            std::forward<Ts>(args)...));
  }

  template <class Token, typename F, typename T0, class... Ts>
  void set(Token t1, std::false_type t2, F fun, T0&& arg0, Ts&&... args) {
    set(t1, t2,
      std::bind(fun, std::forward<T0>(arg0), std::forward<Ts>(args)...));
  }

  // we convert any of the three accepted signatures to this one
  one_arg_fun1 m_fun;
};

/**
 * Infers the appropriate base class for a functor-based typed actor
 * from the result and the first argument of the functor.
 */
template <class Result, class FirstArg>
struct infer_typed_actor_base;

template <class... Rs, class FirstArg>
struct infer_typed_actor_base<typed_behavior<Rs...>, FirstArg> {
  using type = functor_based_typed_actor<Rs...>;
};

template <class... Rs>
struct infer_typed_actor_base<void, typed_event_based_actor<Rs...>*> {
  using type = functor_based_typed_actor<Rs...>;
};

/**
 * Returns a new typed actor of type `C` using `args...` as
 * constructor arguments.
 */
template <class C, spawn_options Os = no_spawn_options, class... Ts>
typename actor_handle_from_signature_list<typename C::signatures>::type
spawn_typed(Ts&&... args) {
  return spawn_class<C, Os>(nullptr, empty_before_launch_callback{},
                            std::forward<Ts>(args)...);
}

/**
 * Spawns a typed actor from a functor .
 */
template <spawn_options Os, typename BeforeLaunch, typename F, class... Ts>
typename infer_typed_actor_handle<
  typename detail::get_callable_trait<F>::result_type,
  typename detail::tl_head<
    typename detail::get_callable_trait<F>::arg_types
  >::type
>::type
spawn_typed_functor(execution_unit* eu, BeforeLaunch bl, F fun, Ts&&... args) {
  using impl =
    typename infer_typed_actor_base<
      typename detail::get_callable_trait<F>::result_type,
      typename detail::tl_head<
        typename detail::get_callable_trait<F>::arg_types
      >::type
    >::type;
  return spawn_class<impl, Os>(eu, bl, fun, std::forward<Ts>(args)...);
}

/**
 * Returns a new typed actor from a functor. The first element
 * of `args` must be the functor, the remaining arguments are used to
 * invoke the functor. This function delegates its arguments to
 * `spawn_typed_functor`.
 */
template <spawn_options Os = no_spawn_options, typename F, class... Ts>
typename infer_typed_actor_handle<
  typename detail::get_callable_trait<F>::result_type,
  typename detail::tl_head<
    typename detail::get_callable_trait<F>::arg_types
  >::type
>::type
spawn_typed(F fun, Ts&&... args) {
  return spawn_typed_functor<Os>(nullptr, empty_before_launch_callback{},
                                 std::move(fun), std::forward<Ts>(args)...);
}

/** @} */

} // namespace caf

#endif // CAF_SPAWN_HPP
