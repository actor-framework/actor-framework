// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/actor_system_access.hpp"

#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/scheduler.hpp"

namespace caf::detail {

void actor_system_access::logger(intrusive_ptr<caf::logger> ptr) {
  sys_->set_logger(std::move(ptr));
}

void actor_system_access::clock(std::unique_ptr<actor_clock> ptr) {
  sys_->set_clock(std::move(ptr));
}

void actor_system_access::scheduler(std::unique_ptr<caf::scheduler> ptr) {
  sys_->set_scheduler(std::move(ptr));
}

void actor_system_access::printer(strong_actor_ptr ptr) {
  sys_->set_legacy_printer_actor(std::move(ptr));
}

void actor_system_access::node(node_id id) {
  sys_->set_node(id);
}

detail::mailbox_factory* actor_system_access::mailbox_factory() {
  return sys_->mailbox_factory();
}

} // namespace caf::detail
