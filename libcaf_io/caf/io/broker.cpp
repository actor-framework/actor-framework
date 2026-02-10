// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/io/broker.hpp"

#include "caf/io/middleman.hpp"

#include "caf/actor_registry.hpp"
#include "caf/config.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/detail/sync_request_bouncer.hpp"
#include "caf/log/io.hpp"
#include "caf/make_counted.hpp"
#include "caf/none.hpp"

namespace caf::io {

behavior broker::make_behavior() {
  behavior res;
  if (initial_behavior_fac_) {
    res = initial_behavior_fac_(this);
    initial_behavior_fac_ = nullptr;
  }
  return res;
}

behavior broker::type_erased_initial_behavior() {
  init_broker();
  return make_behavior();
}

} // namespace caf::io
