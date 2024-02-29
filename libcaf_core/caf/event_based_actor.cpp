// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/event_based_actor.hpp"

#include "caf/detail/pretty_type_name.hpp"
#include "caf/log/core.hpp"
#include "caf/message_id.hpp"

namespace caf {

event_based_actor::event_based_actor(actor_config& cfg) : extended_base(cfg) {
  // nop
}

event_based_actor::~event_based_actor() {
  // nop
}

void event_based_actor::initialize() {
  auto lg = log::core::trace("subtype = {}",
                             detail::pretty_type_name(typeid(*this)).c_str());
  extended_base::initialize();
  setf(is_initialized_flag);
  auto bhvr = make_behavior();
  if (!bhvr) {
    log::core::debug("make_behavior() did not return a behavior: alive = {}",
                     alive());
  }
  if (bhvr) {
    // make_behavior() did return a behavior instead of using become()
    log::core::debug("make_behavior() did return a valid behavior");
    become(std::move(bhvr));
  }
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

} // namespace caf
