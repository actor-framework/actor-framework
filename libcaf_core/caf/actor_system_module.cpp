// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_system_module.hpp"

namespace caf {

namespace {

const char* module_names[] = {
  "middleman",
  "openssl-manager",
  "network-manager",
  "daemons",
};

} // namespace

actor_system_module::~actor_system_module() {
  // nop
}

const char* actor_system_module::name() const noexcept {
  auto index = static_cast<int>(id());
  if (index < num_ids) {
    return module_names[index];
  }
  return "???";
}

} // namespace caf
