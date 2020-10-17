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

#include <new>
#include <type_traits>

#include "caf/fwd.hpp"
#include "caf/sec.hpp"
#include "caf/unsafe_behavior_init.hpp"

#include "caf/detail/type_traits.hpp"

namespace caf::detail {

/// Conditional base type for `stateful_actor` that overrides `make_behavior` if
/// `State::make_behavior()` exists.
template <class State, class Base>
class stateful_actor_base : public Base {
public:
  using Base::Base;

  typename Base::behavior_type make_behavior() override;
};

/// Evaluates to either `stateful_actor_base<State, Base> `or `Base`, depending
/// on whether `State::make_behavior()` exists.
template <class State, class Base>
using stateful_actor_base_t
  = std::conditional_t<has_make_behavior_member<State>::value,
                       stateful_actor_base<State, Base>, Base>;

} // namespace caf::detail

namespace caf {

/// An event-based actor with managed state. The state is constructed with the
/// actor, but destroyed when the actor calls `quit`. This state management
/// brakes cycles and allows actors to automatically release resources as soon
/// as possible.
template <class State, class Base /* = event_based_actor (see fwd.hpp) */>
class stateful_actor : public detail::stateful_actor_base_t<State, Base> {
public:
  using super = detail::stateful_actor_base_t<State, Base>;

  template <class... Ts>
  explicit stateful_actor(actor_config& cfg, Ts&&... xs) : super(cfg) {
    if constexpr (std::is_constructible<State, Ts&&...>::value)
      new (&state) State(std::forward<Ts>(xs)...);
    else
      new (&state) State(this, std::forward<Ts>(xs)...);
  }

  ~stateful_actor() override {
    // nop
  }

  /// @copydoc local_actor::on_exit
  /// @note when overriding this member function, make sure to call
  ///       `super::on_exit()` in order to clean up the state.
  void on_exit() override {
    state.~State();
  }

  const char* name() const override {
    if constexpr (detail::has_name<State>::value) {
      if constexpr (!std::is_member_pointer<decltype(&State::name)>::value) {
        if constexpr (std::is_convertible<decltype(State::name),
                                          const char*>::value) {
          return State::name;
        }
      } else {
        non_static_name_member(state.name);
      }
    }
    return Base::name();
  }

  union {
    /// The actor's state. This member lives inside a union since its lifetime
    /// ends when the actor terminates while the actual actor object lives until
    /// its reference count drops to zero.
    State state;
  };

  template <class T>
  [[deprecated("non-static 'State::name' members have no effect since 0.18")]]
  // This function only exists to raise a deprecated warning.
  static void
  non_static_name_member(const T&) {
    // nop
  }
};

} // namespace caf

namespace caf::detail {

template <class State, class Base>
typename Base::behavior_type stateful_actor_base<State, Base>::make_behavior() {
  // When spawning function-based actors, CAF sets `initial_behavior_fac_` to
  // wrap the function invocation. This always has the highest priority.
  if (this->initial_behavior_fac_) {
    auto res = this->initial_behavior_fac_(this);
    this->initial_behavior_fac_ = nullptr;
    return {unsafe_behavior_init, std::move(res)};
  }
  auto dptr = static_cast<stateful_actor<State, Base>*>(this);
  return dptr->state.make_behavior();
}

} // namespace caf::detail
