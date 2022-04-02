// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/subscription.hpp"

namespace caf::flow {

subscription::impl::~impl() {
  // nop
}

void subscription::impl::dispose() {
  cancel();
}

bool subscription::nop_impl::disposed() const noexcept {
  return disposed_;
}

void subscription::nop_impl::ref_disposable() const noexcept {
  ref();
}

void subscription::nop_impl::deref_disposable() const noexcept {
  deref();
}

void subscription::nop_impl::cancel() {
  disposed_ = true;
}

void subscription::nop_impl::request(size_t) {
  // nop
}

} // namespace caf::flow
