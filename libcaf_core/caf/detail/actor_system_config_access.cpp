// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/actor_system_config_access.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/detail/mailbox_factory.hpp"

namespace caf::detail {

void actor_system_config_access::mailbox_factory(
  std::unique_ptr<detail::mailbox_factory> factory) {
  cfg_->mailbox_factory(std::move(factory));
}

} // namespace caf::detail
