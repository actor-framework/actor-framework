// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/action.hpp"

#include "caf/logger.hpp"

namespace caf {

resumable::subtype_t action::impl::subtype() const noexcept {
  return resumable::function_object;
}

void action::impl::ref_resumable() const noexcept {
  ref_disposable();
}

void action::impl::deref_resumable() const noexcept {
  deref_disposable();
}

action::action(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
  // nop
}

} // namespace caf
