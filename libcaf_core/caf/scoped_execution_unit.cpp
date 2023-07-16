// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/scoped_execution_unit.hpp"

#include "caf/actor_system.hpp"
#include "caf/scheduler/abstract_coordinator.hpp"

namespace caf {

scoped_execution_unit::~scoped_execution_unit() {
  // nop
}

void scoped_execution_unit::exec_later(resumable* ptr) {
  system().scheduler().enqueue(ptr);
}

} // namespace caf
