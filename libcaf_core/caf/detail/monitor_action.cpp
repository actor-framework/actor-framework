// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/monitor_action.hpp"

namespace caf::detail {

void abstract_monitor_action::ref_disposable() const noexcept {
  ref();
}

void abstract_monitor_action::deref_disposable() const noexcept {
  deref();
}

} // namespace caf::detail
