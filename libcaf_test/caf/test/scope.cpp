// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/scope.hpp"

#include "caf/test/block.hpp"

namespace caf::test {

scope& scope::operator=(scope&& other) noexcept {
  if (ptr_)
    ptr_->leave();
  ptr_ = other.release();
  return *this;
}

scope::~scope() {
  if (ptr_)
    ptr_->leave();
}

void scope::leave() {
  if (ptr_) {
    ptr_->on_leave();
    ptr_->leave();
    ptr_ = nullptr;
  }
}

} // namespace caf::test
