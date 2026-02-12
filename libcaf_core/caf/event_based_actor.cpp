// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/event_based_actor.hpp"

#include "caf/detail/pretty_type_name.hpp"
#include "caf/log/core.hpp"
#include "caf/message_id.hpp"

namespace caf {

event_based_actor::event_based_actor(actor_config& cfg) : super(cfg) {
  // nop
}

event_based_actor::~event_based_actor() {
  // nop
}

behavior event_based_actor::make_behavior() {
  auto lg = log::core::trace("");
  behavior res;
  if (initial_behavior_fac_) {
    res = initial_behavior_fac_(this);
    initial_behavior_fac_ = nullptr;
  }
  return res;
}

behavior event_based_actor::type_erased_initial_behavior() {
  return make_behavior();
}

} // namespace caf
