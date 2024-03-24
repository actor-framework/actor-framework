// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_system_module.hpp"

namespace caf {

actor_system_module::~actor_system_module() {
  // nop
}

const char* actor_system_module::name() const noexcept {
  switch (id()) {
    case middleman:
      return "middleman";
    case openssl_manager:
      return "openssl-manager";
    case network_manager:
      return "network-manager";
    default:
      return "???";
  }
}

} // namespace caf
