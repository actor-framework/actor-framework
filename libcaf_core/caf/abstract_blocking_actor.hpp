// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

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
  template <class>
  friend class blocking_response_handle;

  using super = local_actor;

  using super::super;

  ~abstract_blocking_actor() override;

private:
  virtual void do_receive(message_id mid, behavior& bhvr, timespan timeout) = 0;
};

} // namespace caf
