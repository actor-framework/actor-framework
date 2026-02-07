// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/actor_system_access.hpp"

#include "caf/actor_system.hpp"

namespace caf::detail {

void actor_system_access::node(node_id id) {
  sys_->set_node(id);
}

detail::mailbox_factory* actor_system_access::mailbox_factory() {
  return sys_->mailbox_factory();
}

} // namespace caf::detail
