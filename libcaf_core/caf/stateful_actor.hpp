// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/behavior.hpp"
#include "caf/callback.hpp"
#include "caf/detail/concepts.hpp"
#include "caf/detail/metaprogramming.hpp"
#include "caf/fwd.hpp"
#include "caf/unsafe_behavior_init.hpp"

#include <new>
#include <tuple>
#include <type_traits>

namespace caf::detail {

/// Conditional base type for `stateful_actor` that overrides `make_behavior` if
/// `State::make_behavior()` exists.
template <class State, class Base>
class stateful_actor_base : public Base {
public:
  using Base::Base;

  typename Base::behavior_type make_behavior() override;
};

/// Conditional base type for `stateful_actor` that overrides `act` when using a
/// blocking actor as base.
template <class State, class Base>
class stateful_blocking_actor_base : public Base {
public:
  using Base::Base;

  void act() override;
};

template <class State, class Base>
struct stateful_actor_base_oracle {
  using type = Base;
};

template <has_make_behavior State, non_blocking_actor_type Base>
struct stateful_actor_base_oracle<State, Base> {
  using type = stateful_actor_base<State, Base>;
};

template <has_make_behavior State, blocking_actor_type Base>
struct stateful_actor_base_oracle<State, Base> {
  using type = stateful_blocking_actor_base<State, Base>;
};

/// Evaluates to either `stateful_actor_base<State, Base>`,
/// `stateful_blocking_actor_base<State, Base>`, or `Base`, depending on whether
/// the state has `make_behavior()` and whether the base is a blocking actor.
template <class State, class Base>
using stateful_actor_base_t =
  typename stateful_actor_base_oracle<State, Base>::type;

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
    if constexpr (std::is_constructible_v<State, Ts&&...>)
      new (&state_) State(std::forward<Ts>(xs)...);
    else
      new (&state_) State(this, std::forward<Ts>(xs)...);
  }

  ~stateful_actor() override {
    // nop
  }

  /// @copydoc local_actor::on_exit
  /// @note when overriding this member function, make sure to call
  ///       `super::on_exit()` in order to clean up the state.
  void on_exit() override {
    state_.~State();
  }

  const char* name() const override {
    if constexpr (detail::has_name<State>) {
      if constexpr (!std::is_member_pointer<decltype(&State::name)>::value) {
        if constexpr (std::is_convertible<decltype(State::name),
                                          const char*>::value) {
          return State::name;
        }
      }
    }
    return Base::name();
  }

  /// Returns a reference to the actor's state.
  State& state() {
    return state_;
  }

private:
  union {
    /// The actor's state. This member lives inside a union since its lifetime
    /// ends when the actor terminates while the actual actor object lives until
    /// its reference count drops to zero.
    State state_;
  };
};

} // namespace caf

namespace caf::detail {

template <class State, class Base>
typename Base::behavior_type stateful_actor_base<State, Base>::make_behavior() {
  auto dptr = static_cast<stateful_actor<State, Base>*>(this);
  return dptr->state().make_behavior();
}

template <class State, class Base>
void stateful_blocking_actor_base<State, Base>::act() {
  // We call `make_behavior()` only to invoke the user-defined callback. For
  // blocking actors, this callback must return `void` and `make_behavior()`
  // will thus always return a default-constructed (empty) behavior that we can
  // safely discard.
  auto dptr = static_cast<stateful_actor<State, Base>*>(this);
  std::ignore = dptr->state().make_behavior();
}

template <class Self>
struct functor_state {
  using behavior_type = typename Self::behavior_type;

  template <class Fn, class... Args>
  explicit functor_state(Self* selfptr, Fn&& f, Args&&... args)
    : self(selfptr) {
    // Wrap the function invocation into a callback that we can call later in
    // `make_behavior()` when launching the actor.
    if constexpr (std::is_invocable_r_v<behavior_type, Fn, Self*, Args...>) {
      static_assert(!actor_traits<Self>::is_blocking,
                    "Blocking actors cannot return a behavior");
      auto g = [f = std::forward<Fn>(f), ... args = std::forward<Args>(args)](
                 Self* self) mutable -> behavior_type {
        return f(self, std::move(args)...);
      };
      fn = make_type_erased_callback(std::move(g));
    } else if constexpr (std::is_invocable_r_v<behavior_type, Fn, Args...>) {
      static_assert(!actor_traits<Self>::is_blocking,
                    "Blocking actors cannot return a behavior");
      auto g = [f = std::forward<Fn>(f), ... args = std::forward<Args>(args)](
                 Self*) mutable -> behavior_type {
        return f(std::move(args)...);
      };
      fn = make_type_erased_callback(std::move(g));
    } else if constexpr (std::is_invocable_r_v<void, Fn, Self*, Args...>) {
      auto g = [f = std::forward<Fn>(f), ... args = std::forward<Args>(args)](
                 Self* self) mutable -> behavior_type {
        f(self, std::move(args)...);
        return behavior_type{unsafe_behavior_init};
      };
      fn = make_type_erased_callback(std::move(g));
    } else if constexpr (std::is_invocable_r_v<void, Fn, Args...>) {
      auto g = [f = std::forward<Fn>(f), ... args = std::forward<Args>(args)](
                 Self*) mutable -> behavior_type {
        f(std::move(args)...);
        return behavior_type{unsafe_behavior_init};
      };
      fn = make_type_erased_callback(std::move(g));
    } else {
      static_assert(detail::always_false<Fn>, "Invalid callable type");
    }
  }

  behavior_type make_behavior() {
    if (fn) {
      auto res = (*fn)(self);
      fn = nullptr;
      return res;
    }
    return behavior_type{unsafe_behavior_init};
  }

  Self* self;
  unique_callback_ptr<behavior_type(Self*)> fn;
};

} // namespace caf::detail
