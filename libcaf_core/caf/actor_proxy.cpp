// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_proxy.hpp"

namespace caf {

actor_proxy::actor_proxy(actor_config& cfg) : monitorable_actor(cfg) {
  // nop
}

actor_proxy::~actor_proxy() {
  // nop
}

} // namespace caf
