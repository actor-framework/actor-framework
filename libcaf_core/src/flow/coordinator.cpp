// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/coordinator.hpp"

#include "caf/config.hpp"
#include "caf/flow/observable_builder.hpp"

namespace caf::flow {

coordinator::~coordinator() {
  // nop
}

observable_builder coordinator::make_observable() {
  return observable_builder{this};
}

} // namespace caf::flow
