// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/plain_ref_counted.hpp"

namespace caf::detail {

plain_ref_counted::~plain_ref_counted() {
  // nop
}

plain_ref_counted::plain_ref_counted() : rc_(1) {
  // nop
}

plain_ref_counted::plain_ref_counted(const plain_ref_counted&) : rc_(1) {
  // Intentionally don't copy the reference count.
}

plain_ref_counted& plain_ref_counted::operator=(const plain_ref_counted&) {
  // Intentionally don't copy the reference count.
  return *this;
}

} // namespace caf::detail
