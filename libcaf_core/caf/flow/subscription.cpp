// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/flow/subscription.hpp"

#include "caf/flow/coordinator.hpp"

namespace caf::flow {

subscription::impl::~impl() {
  // nop
}

void subscription::impl_base::dispose() {
  if (!disposed()) {
    parent()->delay_fn([sptr = intrusive_ptr<impl_base>{this, add_ref}] { //
      sptr->do_dispose(true);
    });
  }
}

void subscription::impl_base::cancel() {
  do_dispose(false);
}

void subscription::trivial_impl::ref() const noexcept {
  ref_count_.inc();
}

void subscription::trivial_impl::deref() const noexcept {
  ref_count_.dec(this);
}

coordinator* subscription::trivial_impl::parent() const noexcept {
  return parent_;
}

bool subscription::trivial_impl::disposed() const noexcept {
  return disposed_;
}

void subscription::trivial_impl::do_dispose(bool) {
  disposed_ = true;
}

void subscription::trivial_impl::request(size_t) {
  // nop
}

} // namespace caf::flow
