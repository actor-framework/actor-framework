// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/monitor_action.hpp"

#include "caf/abstract_actor.hpp"
#include "caf/actor_cast.hpp"
#include "caf/internal/attachable_predicate.hpp"

namespace caf::detail {

abstract_monitor_action::~abstract_monitor_action() noexcept {
  // nop
}

void abstract_monitor_action::ref() const noexcept {
  ref_count_.inc();
}

void abstract_monitor_action::deref() const noexcept {
  ref_count_.dec(this);
}

void abstract_monitor_action::on_dispose() {
  if (auto observer = observer_.lock()) {
    if (auto observed = observed_.lock()) {
      auto* self = actor_cast<abstract_actor*>(observer);
      auto pred = internal::attachable_predicate::monitored_with_callback(this);
      self->del_monitor(actor_cast<abstract_actor*>(observed), pred);
    }
  }
}

} // namespace caf::detail
