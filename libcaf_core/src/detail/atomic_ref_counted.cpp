// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/atomic_ref_counted.hpp"

namespace caf::detail {

atomic_ref_counted::~atomic_ref_counted() {
  // nop
}

atomic_ref_counted::atomic_ref_counted() : rc_(1) {
  // nop
}

atomic_ref_counted::atomic_ref_counted(const atomic_ref_counted&) : rc_(1) {
  // Intentionally don't copy the reference count.
}

atomic_ref_counted& atomic_ref_counted::operator=(const atomic_ref_counted&) {
  // Intentionally don't copy the reference count.
  return *this;
}

void atomic_ref_counted::deref() const noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete this;
}

} // namespace caf::detail
