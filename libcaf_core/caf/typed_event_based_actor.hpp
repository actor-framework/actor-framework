// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_system.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_behavior.hpp"

namespace caf {

/// A cooperatively scheduled, event-based actor
/// implementation with static type-checking.
/// @extends local_actor
template <class... Sigs>
class typed_event_based_actor
  : public extend<scheduled_actor, typed_event_based_actor<Sigs...>>::
      template with<mixin::sender, mixin::requester>,
    public statically_typed_actor_base {
public:
  // -- member types -----------------------------------------------------------

  using super =
    typename extend<scheduled_actor, typed_event_based_actor<Sigs...>>::
      template with<mixin::sender, mixin::requester>;

  using signatures = detail::type_list<Sigs...>;

  using behavior_type = typed_behavior<Sigs...>;

  using actor_hdl = typed_actor<Sigs...>;

  struct trait {
    using signatures = detail::type_list<Sigs...>;
  };

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- overrides --------------------------------------------------------------

  std::set<std::string> message_types() const override {
    detail::type_list<typed_actor<Sigs...>> token;
    return this->system().message_types(token);
  }

  void initialize() override {
    CAF_LOG_TRACE("");
    super::initialize();
    this->setf(abstract_actor::is_initialized_flag);
    auto bhvr = make_behavior();
    CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                              << CAF_ARG2("alive", this->alive()));
    if (bhvr) {
      // make_behavior() did return a behavior instead of using become()
      CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
      this->do_become(std::move(bhvr.unbox()), true);
    }
  }

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  auto mail(Args&&... args) {
    return event_based_mail(trait{}, this, std::forward<Args>(args)...);
  }

  // -- behavior management ----------------------------------------------------

  /// @copydoc event_based_actor::become
  template <class T, class... Ts>
  void become(T&& arg, Ts&&... args) {
    if constexpr (std::is_same_v<keep_behavior_t, std::decay_t<T>>) {
      static_assert(sizeof...(Ts) > 0);
      this->do_become(behavior_type{std::forward<Ts>(args)...}.unbox(), false);
    } else {
      behavior_type bhv{std::forward<T>(arg), std::forward<Ts>(args)...};
      this->do_become(std::move(bhv).unbox(), true);
    }
  }

  /// @copydoc event_based_actor::unbecome
  void unbecome() {
    this->bhvr_stack_.pop_back();
  }

protected:
  virtual behavior_type make_behavior() {
    if (this->initial_behavior_fac_) {
      auto bhvr = this->initial_behavior_fac_(this);
      this->initial_behavior_fac_ = nullptr;
      if (bhvr)
        this->do_become(std::move(bhvr), true);
    }
    return behavior_type::make_empty_behavior();
  }
};

} // namespace caf
