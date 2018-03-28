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

#include <unordered_set>

#include "caf/fwd.hpp"
#include "caf/group.hpp"

namespace caf {
namespace mixin {

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
    auto me = dptr()->ctrl();
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
    if (what->subscribe(dptr()->ctrl()))
      subscriptions_.emplace(what);
  }

  /// Causes this actor to leave the group `what`.
  void leave(const group& what) {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (subscriptions_.erase(what) > 0)
      what->unsubscribe(dptr()->ctrl());
  }

  /// Returns all subscribed groups.
  const subscriptions& joined_groups() const {
    return subscriptions_;
  }

private:
  Subtype* dptr() {
    return static_cast<Subtype*>(this);
  }

  // -- data members -----------------------------------------------------------

  /// Stores all subscribed groups.
  subscriptions subscriptions_;
};

} // namespace mixin
} // namespace caf

