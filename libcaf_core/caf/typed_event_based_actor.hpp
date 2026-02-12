// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/to_statically_typed_trait.hpp"
#include "caf/event_based_mail.hpp"
#include "caf/extend.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/scheduled_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_behavior.hpp"

namespace caf {

/// A cooperatively scheduled, event-based actor implementation with static
/// type-checking.
/// @extends scheduled actor
template <class... Ts>
  requires typed_actor_pack<Ts...>
class typed_event_based_actor : public scheduled_actor,
                                public statically_typed_actor_base

{
public:
  // -- member types -----------------------------------------------------------

  using super = scheduled_actor;

  using trait = detail::to_statically_typed_trait_t<Ts...>;

  using signatures = typename trait::signatures;

  using behavior_type = typed_behavior<trait>;

  using actor_hdl = typed_actor<trait>;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  // -- overrides --------------------------------------------------------------

  std::set<std::string> message_types() const override {
    type_list<typed_actor<trait>> token;
    return this->system().message_types(token);
  }

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  auto mail(Args&&... args) {
    return event_based_mail(trait{}, this, std::forward<Args>(args)...);
  }

  CAF_ADD_DEPRECATED_REQUEST_API

  // -- behavior management ----------------------------------------------------

  /// @copydoc caf::event_based_actor::become
  template <class Arg, class... Args>
  void become(Arg&& arg, Args&&... args) {
    if constexpr (std::is_same_v<keep_behavior_t, std::decay_t<Arg>>) {
      static_assert(sizeof...(Args) > 0);
      this->do_become(behavior_type{std::forward<Args>(args)...}.unbox(),
                      false);
    } else {
      behavior_type bhv{std::forward<Arg>(arg), std::forward<Args>(args)...};
      this->do_become(std::move(bhv).unbox(), true);
    }
  }

  /// @copydoc caf::event_based_actor::unbecome
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

private:
  behavior type_erased_initial_behavior() final {
    return make_behavior().unbox();
  }
};

} // namespace caf
