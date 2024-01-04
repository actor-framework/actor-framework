// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <new>
#include <type_traits>
#include <utility>

namespace caf::detail {

/// An event-based actor with managed state. The state is constructed separately
/// and destroyed when the actor calls `quit`.
template <class State, class Base>
class actor_from_state_impl : public Base {
public:
  using super = Base;

  friend actor_from_state_t<State>;

  explicit actor_from_state_impl(actor_config& cfg) : super(cfg) {
    // nop
  }

  ~actor_from_state_impl() override {
    // nop; must be implemented due to the union member
  }

  void on_exit() override {
    if (has_state_)
      state.~State();
  }

  union {
    State state;
  };

private:
  bool has_state_ = false;
};

template <class T>
struct actor_from_state_impl_base;

template <>
struct actor_from_state_impl_base<behavior> {
  using type = event_based_actor;
};

template <class... Ts>
struct actor_from_state_impl_base<typed_behavior<Ts...>> {
  using type = typed_event_based_actor<Ts...>;
};

} // namespace caf::detail

namespace caf {

/// Helper class for automating the creation of an event-based actor with
/// managed state.
template <class State>
struct actor_from_state_t {
  /// The behavior type of the actor.
  using behavior_type = decltype(std::declval<State*>()->make_behavior());

  static_assert(detail::is_behavior_v<behavior_type>,
                "State::make_behavior() must return a behavior");

  /// The base class for the actor implementation. Either `event_based_actor`
  /// or `typed_event_based_actor`.
  using base_type =
    typename detail::actor_from_state_impl_base<behavior_type>::type;

  /// The actual actor implementation.
  using impl_type = detail::actor_from_state_impl<State, base_type>;

  /// Constructs the state from the given arguments and returns the initial
  /// behavior for the actor.
  template <class... Args>
  behavior_type operator()(impl_type* self, Args&&... args) const {
    if constexpr (std::is_constructible_v<State, Args...>) {
      new (&self->state) State(std::forward<Args>(args)...);
    } else {
      static_assert(std::is_constructible_v<State, base_type*, Args...>,
                    "cannot construct state from given arguments");
      auto ptr = static_cast<base_type*>(self);
      new (&self->state) State(ptr, std::forward<Args>(args)...);
    }
    self->has_state_ = true;
    return self->state.make_behavior();
  }
};

/// A function object that automates the creation of an event-based actor with
/// managed state.
template <class State>
inline constexpr auto actor_from_state = actor_from_state_t<State>{};

} // namespace caf
