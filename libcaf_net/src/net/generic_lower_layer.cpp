// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/net/generic_lower_layer.hpp"

namespace caf::net {

generic_lower_layer::~generic_lower_layer() {
  // nop
}

void generic_lower_layer::shutdown(const error&) {
  shutdown();
}

} // namespace caf::net
