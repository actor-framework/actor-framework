// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/flow/subscription.hpp"

#include "caf/flow/coordinator.hpp"

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

subscription::listener::~listener() {
  // nop
}

bool subscription::default_impl::disposed() const noexcept {
  return src_ == nullptr;
}

void subscription::default_impl::ref_disposable() const noexcept {
  this->ref();
}

void subscription::default_impl::deref_disposable() const noexcept {
  this->deref();
}

void subscription::default_impl::request(size_t n) {
  if (src_)
    ctx()->delay_fn([src = src_, snk = snk_, n] { //
      src->on_request(snk.get(), n);
    });
}

void subscription::default_impl::cancel() {
  if (src_) {
    ctx()->delay_fn([src = src_, snk = snk_] { //
      src->on_cancel(snk.get());
    });
    src_.reset();
    snk_.reset();
  }
}

} // namespace caf::flow
