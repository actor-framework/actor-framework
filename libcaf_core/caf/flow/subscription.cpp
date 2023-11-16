// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/subscription.hpp"

#include "caf/flow/coordinator.hpp"

namespace caf::flow {

subscription::impl::~impl() {
  // nop
}

void subscription::impl_base::ref_disposable() const noexcept {
  this->ref();
}

void subscription::impl_base::deref_disposable() const noexcept {
  this->deref();
}

void subscription::impl_base::ref_coordinated() const noexcept {
  this->ref();
}

void subscription::impl_base::deref_coordinated() const noexcept {
  this->deref();
}

void subscription::impl_base::dispose() {
  if (!disposed()) {
    parent()->delay_fn([sptr = intrusive_ptr<impl_base>{this}] { //
      sptr->do_dispose(true);
    });
  }
}

void subscription::impl_base::cancel() {
  do_dispose(false);
}

subscription::listener::~listener() {
  // nop
}

bool subscription::fwd_impl::disposed() const noexcept {
  return src_ == nullptr;
}

void subscription::fwd_impl::do_dispose(bool from_external) {
  if (src_) {
    auto src = std::move(src_);
    auto snk = std::move(snk_);
    if (from_external)
      src->on_dispose(snk.get());
    else
      src->on_cancel(snk.get());
  }
}

void subscription::fwd_impl::request(size_t n) {
  if (src_)
    parent()->delay_fn([src = src_, snk = snk_, n] { //
      src->on_request(snk.get(), n);
    });
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
