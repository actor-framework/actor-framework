// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/resumable.hpp"

#include "caf/ref_counted.hpp"

namespace caf {

resumable::~resumable() {
  // nop
}

scheduler* resumable::pinned_scheduler() const noexcept {
  return nullptr;
}

} // namespace caf
