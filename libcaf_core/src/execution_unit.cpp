// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/execution_unit.hpp"

namespace caf {

execution_unit::execution_unit(actor_system* sys)
  : system_(sys), proxies_(nullptr) {
  // nop
}

execution_unit::~execution_unit() {
  // nop
}

} // namespace caf
