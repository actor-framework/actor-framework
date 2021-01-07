// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <unordered_set>

#include "caf/fwd.hpp"
#include "caf/group.hpp"

namespace caf::mixin {

/// Marker for `subscriber`.
struct subscriber_base {};

/// A `subscriber` is an actor that can subscribe
/// to a `group` via `self->join(...)`.
template <class Base, class Subtype>
class subscriber : public Base, public subscriber_base {
public:
  // -- member types -----------------------------------------------------------

  /// Allows subtypes to refer mixed types with a simple name.
  using extended_base = subscriber;

  /// A container for storing subscribed groups.
  using subscriptions = std::unordered_set<group>;

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  subscriber(actor_config& cfg, Ts&&... xs)
    : Base(cfg, std::forward<Ts>(xs)...) {
    if (cfg.groups != nullptr)
      for (auto& grp : *cfg.groups)
        join(grp);
  }

  // -- overridden functions of monitorable_actor ------------------------------

  bool cleanup(error&& fail_state, execution_unit* ptr) override {
    auto me = this->ctrl();
    for (auto& subscription : subscriptions_)
      subscription->unsubscribe(me);
    subscriptions_.clear();
    return Base::cleanup(std::move(fail_state), ptr);
  }

  // -- group management -------------------------------------------------------

  /// Causes this actor to subscribe to the group `what`.
  /// The group will be unsubscribed if the actor finishes execution.
  void join(const group& what) {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (what == invalid_group)
      return;
    if (what->subscribe(this->ctrl()))
      subscriptions_.emplace(what);
  }

  /// Causes this actor to leave the group `what`.
  void leave(const group& what) {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (subscriptions_.erase(what) > 0)
      what->unsubscribe(this->ctrl());
  }

  /// Returns all subscribed groups.
  const subscriptions& joined_groups() const {
    return subscriptions_;
  }

private:
  // -- data members -----------------------------------------------------------

  /// Stores all subscribed groups.
  subscriptions subscriptions_;
};

} // namespace caf::mixin
