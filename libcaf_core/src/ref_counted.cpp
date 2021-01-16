// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/ref_counted.hpp"

namespace caf {

ref_counted::~ref_counted() {
  // nop
}

ref_counted::ref_counted() : rc_(1) {
  // nop
}

ref_counted::ref_counted(const ref_counted&) : rc_(1) {
  // nop; don't copy reference count
}

ref_counted& ref_counted::operator=(const ref_counted&) {
  // nop; intentionally don't copy reference count
  return *this;
}

void ref_counted::deref() const noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete this;
}

} // namespace caf
