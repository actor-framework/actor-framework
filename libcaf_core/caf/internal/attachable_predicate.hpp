// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_addr.hpp"
#include "caf/disposable.hpp"
#include "caf/fwd.hpp"

#include <utility>
#include <variant>

namespace caf::internal {

class monitor_attachable;
class monitor_action_attachable;
class link_attachable;

/// Base class for predicates operating on any of the default attachable types.
class attachable_predicate {
public:
  /// Utility struct for extracting the observer from an attachable on match.
  struct extractor {
    const actor_addr* addr;
    weak_actor_ptr* result;
  };

  /// Predicate for checking whether an observer actor is monitoring the
  /// observed actor. Only matches monitor attachables.
  struct monitored_by_state {
    const actor_control_block* observer;
  };

  /// Predicate for matching monitor action attachables.
  struct monitored_with_callback_state {
    disposable::impl* impl;
  };

  /// Predicate for checking whether an observer actor is linked to the observed
  /// actor. Only matches link attachables.
  struct linked_to_state {
    const actor_control_block* observer;
  };

  /// Predicate for checking whether an observer actor (identified by its
  /// logical address) is linked to the observed actor. Only matches link
  /// attachables.
  struct linked_to_state_extractor {
    extractor* out;
  };

  /// Predicate for matching attachables by observer in any of the default
  /// attachable types.
  struct observed_by_state {
    const actor_control_block* observer;
  };

  using impl_type = std::variant<monitored_by_state,
                                 monitored_with_callback_state, linked_to_state,
                                 linked_to_state_extractor, observed_by_state>;

  impl_type impl;

  static attachable_predicate
  monitored_by(const actor_control_block* observer) {
    return {std::in_place, monitored_by_state{observer}};
  };

  static attachable_predicate monitored_with_callback(disposable::impl* impl) {
    return {std::in_place, monitored_with_callback_state{impl}};
  };

  static attachable_predicate linked_to(const actor_control_block* observer) {
    return {std::in_place, linked_to_state{observer}};
  };

  static attachable_predicate linked_to(extractor* out) {
    return {std::in_place, linked_to_state_extractor{out}};
  };

  static attachable_predicate observed_by(const actor_control_block* observer) {
    return {std::in_place, observed_by_state{observer}};
  };

  bool operator()(const attachable& what) const;

  bool visit(const monitor_attachable& what) const;

  bool visit(const monitor_action_attachable& what) const;

  bool visit(const link_attachable& what) const;

private:
  template <class State>
  attachable_predicate(std::in_place_t, State state)
    : impl(std::in_place_type<State>, state) {
    // nop
  }
};

// Note: the predicates are implemented in attachable.cpp for technical reasons.

} // namespace caf::internal
