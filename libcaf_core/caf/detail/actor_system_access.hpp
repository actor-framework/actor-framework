// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

namespace caf::detail {

/// Utility to override internal components of an actor system.
class CAF_CORE_EXPORT actor_system_access {
public:
  explicit actor_system_access(actor_system& sys) : sys_(&sys) {
    // nop
  }

  void logger(intrusive_ptr<caf::logger> new_logger, actor_system_config& cfg);

  void clock(std::unique_ptr<actor_clock> new_clock);

  void scheduler(std::unique_ptr<caf::scheduler> new_scheduler);

  void printer(strong_actor_ptr new_printer);

private:
  actor_system* sys_;
};

} // namespace caf::detail
