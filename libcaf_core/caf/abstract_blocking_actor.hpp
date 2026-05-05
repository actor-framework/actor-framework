// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/actor_clock.hpp"
#include "caf/fwd.hpp"
#include "caf/local_actor.hpp"

namespace caf {

/// A thread-mapped or context-switching actor using a blocking
/// receive rather than a behavior-stack based message processing.
/// @extends local_actor
class CAF_CORE_EXPORT abstract_blocking_actor : public local_actor {
public:
  template <class...>
  friend class blocking_response_handle;

  using super = local_actor;

  using super::super;

  ~abstract_blocking_actor() override;

  message_id new_request_id(message_priority mp) noexcept;

protected:
  // last used request ID
  message_id last_request_id_;

private:
  virtual void receive_impl(message_id mid, behavior& bhvr, timespan timeout)
    = 0;
};

} // namespace caf
