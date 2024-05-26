// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/abstract_worker.hpp"

namespace caf::detail {

// -- constructors, destructors, and assignment operators ----------------------

abstract_worker::abstract_worker() : next_(nullptr) {
  // nop
}

abstract_worker::~abstract_worker() {
  // nop
}

// -- implementation of resumable ----------------------------------------------

resumable::subtype_t abstract_worker::subtype() const noexcept {
  return resumable::function_object;
}

void abstract_worker::ref_resumable() const noexcept {
  ref();
}

void abstract_worker::deref_resumable() const noexcept {
  deref();
}

} // namespace caf::detail
