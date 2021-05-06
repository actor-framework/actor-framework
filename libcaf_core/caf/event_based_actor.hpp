// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <type_traits>

#include "caf/actor_traits.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/extend.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"
#include "caf/logger.hpp"
#include "caf/mixin/behavior_changer.hpp"
#include "caf/mixin/requester.hpp"
#include "caf/mixin/sender.hpp"
#include "caf/mixin/subscriber.hpp"
#include "caf/response_handle.hpp"
#include "caf/scheduled_actor.hpp"

namespace caf {

template <>
class behavior_type_of<event_based_actor> {
public:
  using type = behavior;
};

/// A cooperatively scheduled, event-based actor implementation. This is the
/// recommended base class for user-defined actors.
/// @extends scheduled_actor
class CAF_CORE_EXPORT event_based_actor
  // clang-format off
  : public extend<scheduled_actor, event_based_actor>::
           with<mixin::sender,
                mixin::requester,
                mixin::subscriber,
                mixin::behavior_changer>,
    public dynamically_typed_actor_base {
  // clang-format on
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

protected:
  // -- behavior management ----------------------------------------------------

  /// Returns the initial actor behavior.
  virtual behavior make_behavior();
};

} // namespace caf
