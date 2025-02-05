// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/dynamically_typed.hpp"
#include "caf/event_based_mail.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/keep_behavior.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"

#include <type_traits>

namespace caf {

/// A cooperatively scheduled, event-based actor implementation. This is the
/// recommended base class for user-defined actors.
/// @extends scheduled_actor
class CAF_CORE_EXPORT event_based_actor
  : public extend<scheduled_actor, event_based_actor>::with<mixin::requester>,
    public dynamically_typed_actor_base {
public:
  // -- member types -----------------------------------------------------------

  /// Required by `spawn` for type deduction.
  using signatures = none_t;

  /// Required by `spawn` for type deduction.
  using behavior_type = behavior;

  using handle_type = actor;

  // -- constructors, destructors ----------------------------------------------

  explicit event_based_actor(actor_config& cfg);

  ~event_based_actor() override;

  // -- overridden functions of local_actor ------------------------------------

  void initialize() override;

  // -- messaging --------------------------------------------------------------

  /// Starts a new message.
  template <class... Args>
  auto mail(Args&&... args) {
    return event_based_mail(dynamically_typed{}, this,
                            std::forward<Args>(args)...);
  }

  // -- behavior management ----------------------------------------------------

  /// Changes the behavior of this actor.
  template <class T, class... Ts>
  void become(T&& arg, Ts&&... args) {
    if constexpr (std::is_same_v<keep_behavior_t, std::decay_t<T>>) {
      static_assert(sizeof...(Ts) > 0);
      do_become(behavior{std::forward<Ts>(args)...}, false);
    } else {
      do_become(behavior{std::forward<T>(arg), std::forward<Ts>(args)...},
                true);
    }
  }

  /// Removes the last added behavior. Terminates the actor if there are no
  /// behaviors left.
  void unbecome() {
    bhvr_stack_.pop_back();
  }

protected:
  /// Returns the initial actor behavior.
  virtual behavior make_behavior();
};

} // namespace caf
