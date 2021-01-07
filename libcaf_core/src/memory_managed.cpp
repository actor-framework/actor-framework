// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/memory_managed.hpp"

namespace caf {

memory_managed::~memory_managed() {
  // nop
}

void memory_managed::request_deletion(bool) const noexcept {
  delete this;
}

} // namespace caf
