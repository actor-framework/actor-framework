// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"

#include <memory>

namespace caf::detail {

class CAF_CORE_EXPORT actor_system_config_access {
public:
  explicit actor_system_config_access(actor_system_config& cfg) : cfg_(&cfg) {
    // nop
  }

  void mailbox_factory(std::unique_ptr<detail::mailbox_factory> factory);

private:
  actor_system_config* cfg_;
};

} // namespace caf::detail
