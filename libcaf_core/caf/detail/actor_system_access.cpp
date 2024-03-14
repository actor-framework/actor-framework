// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/actor_system_access.hpp"

#include "caf/actor_system.hpp"
#include "caf/logger.hpp"
#include "caf/scheduler.hpp"

namespace caf::detail {

void actor_system_access::logger(intrusive_ptr<caf::logger> new_logger,
                                 actor_system_config& cfg) {
  sys_->logger_ = std::move(new_logger);
  sys_->logger_->init(cfg);
  CAF_SET_LOGGER_SYS(sys_);
}

void actor_system_access::clock(std::unique_ptr<actor_clock> new_clock) {
  sys_->clock_ = std::move(new_clock);
}

void actor_system_access::scheduler(
  std::unique_ptr<caf::scheduler> new_scheduler) {
  sys_->scheduler_ = std::move(new_scheduler);
}

void actor_system_access::printer(strong_actor_ptr new_printer) {
  sys_->printer_ = std::move(new_printer);
}

} // namespace caf::detail
