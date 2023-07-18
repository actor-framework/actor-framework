// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/broker.hpp"

#include "caf/io/middleman.hpp"

#include "caf/actor_registry.hpp"
#include "caf/config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/none.hpp"

namespace caf::io {

void broker::initialize() {
  CAF_LOG_TRACE("");
  init_broker();
  auto bhvr = make_behavior();
  CAF_LOG_DEBUG_IF(!bhvr, "make_behavior() did not return a behavior:"
                            << CAF_ARG2("alive", alive()));
  if (bhvr) {
    // make_behavior() did return a behavior instead of using become()
    CAF_LOG_DEBUG("make_behavior() did return a valid behavior");
    become(std::move(bhvr));
  }
}

broker::broker(actor_config& cfg) : super(cfg) {
  // nop
}

behavior broker::make_behavior() {
  behavior res;
  if (initial_behavior_fac_) {
    res = initial_behavior_fac_(this);
    initial_behavior_fac_ = nullptr;
  }
  return res;
}

} // namespace caf::io
