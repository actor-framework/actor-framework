// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/ref_counted_base.hpp"

namespace caf::detail {

ref_counted_base::~ref_counted_base() {
  // nop
}

ref_counted_base::ref_counted_base() : rc_(1) {
  // nop
}

ref_counted_base::ref_counted_base(const ref_counted_base&) : rc_(1) {
  // nop; intentionally don't copy the reference count
}

ref_counted_base& ref_counted_base::operator=(const ref_counted_base&) {
  // nop; intentionally don't copy the reference count
  return *this;
}

void ref_counted_base::deref() const noexcept {
  if (unique() || rc_.fetch_sub(1, std::memory_order_acq_rel) == 1)
    delete this;
}

} // namespace caf::detail
