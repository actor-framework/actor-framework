// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/actor_traits.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"

namespace caf {

/// A cooperatively scheduled, event-based actor implementation.
class CAF_CORE_EXPORT abstract_scheduled_actor
  : public local_actor,
    public non_blocking_actor_base {
public:
  // -- friends ----------------------------------------------------------------

  template <class...>
  friend class event_based_response_handle;

  // -- member types -----------------------------------------------------------

  using super = local_actor;

  // -- constructors, destructors, and assignment operators --------------------

  using super::super;

  ~abstract_scheduled_actor() override;

  // -- message processing -----------------------------------------------------

  /// Adds a callback for an awaited response.
  virtual void add_awaited_response_handler(message_id response_id,
                                            behavior bhvr,
                                            disposable pending_timeout = {})
    = 0;

  /// Adds a callback for a multiplexed response.
  virtual void add_multiplexed_response_handler(message_id response_id,
                                                behavior bhvr,
                                                disposable pending_timeout = {})
    = 0;

  /// Calls the default error handler.
  virtual void call_error_handler(error& what) = 0;
};

} // namespace caf
