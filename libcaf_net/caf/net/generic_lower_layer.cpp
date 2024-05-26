// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/net/generic_lower_layer.hpp"

#include "caf/net/socket_manager.hpp"

namespace caf::net {

generic_lower_layer::~generic_lower_layer() {
  // nop
}

void generic_lower_layer::shutdown(const error&) {
  shutdown();
}

multiplexer& generic_lower_layer::mpx() noexcept {
  return manager()->mpx();
}

} // namespace caf::net
