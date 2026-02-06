// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/asynchronous_logger.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

class daemons;

/// Utility to override internal components of an actor system.
class CAF_CORE_EXPORT actor_system_access {
public:
  explicit actor_system_access(actor_system& sys) : sys_(&sys) {
    // nop
  }

  void node(node_id id);

  detail::mailbox_factory* mailbox_factory();

  detail::daemons* daemons();

private:
  actor_system* sys_;
};

} // namespace caf::detail
