// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/fwd.hpp"
#include "caf/message_priority.hpp"

namespace caf::internal {

class attachable_factory {
public:
  static attachable_ptr
  make_monitor(actor_addr observer,
               message_priority prio = message_priority::normal);

  static attachable_ptr make_monitor(detail::abstract_monitor_action_ptr ptr);

  static attachable_ptr make_link(actor_addr observer);
};

// Note: implemented in attachable.cpp for technical reasons.

} // namespace caf::internal
