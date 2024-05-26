// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/behavior.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/fwd.hpp"
#include "caf/infer_handle.hpp"
#include "caf/spawn_options.hpp"
#include "caf/stateful_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

namespace caf::detail {

template <class T>
struct actor_base_from_behavior;

template <>
struct actor_base_from_behavior<behavior> {
  using type = event_based_actor;
};

template <class... Ts>
struct actor_base_from_behavior<typed_behavior<Ts...>> {
  using type = typed_event_based_actor<Ts...>;
};

template <class T>
using actor_base_from_behavior_t = typename actor_base_from_behavior<T>::type;

} // namespace caf::detail

namespace caf {

/// Helper class for automating the creation of an event-based actor with
/// managed state.
template <class State>
class actor_from_state_t {
public:
  friend class actor_system;
  friend class local_actor;

  /// The behavior type of the actor.
  using behavior_type = decltype(std::declval<State*>()->make_behavior());

  /// Make sure the state provides a valid behavior type.
  static_assert(detail::is_behavior_v<behavior_type>,
                "State::make_behavior() must return a behavior");

  /// The base class for the actor implementation. Either `event_based_actor`
  /// or `typed_event_based_actor<...>`.
  using base_type = detail::actor_base_from_behavior_t<behavior_type>;

  /// The handle type for the actor.
  using handle_type = infer_handle_from_behavior_t<behavior_type>;

private:
  template <spawn_options Os, class... Args>
  static auto do_spawn(actor_system& sys, actor_config& cfg, Args&&... args) {
    return sys.template spawn_impl<stateful_actor<State, base_type>, Os>(
      cfg, std::forward<Args>(args)...);
  }
};

/// A function object that automates the creation of an event-based actor with
/// managed state.
template <class State>
inline constexpr auto actor_from_state = actor_from_state_t<State>{};

} // namespace caf
