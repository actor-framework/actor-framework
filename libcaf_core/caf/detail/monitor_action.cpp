// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/monitor_action.hpp"

namespace caf::detail {

abstract_monitor_action::~abstract_monitor_action() noexcept {
  // nop
}

} // namespace caf::detail
