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

subscription::listener::~listener() {
  // nop
}

bool subscription::fwd_impl::disposed() const noexcept {
  return src_ == nullptr;
}

void subscription::fwd_impl::request(size_t n) {
  if (src_)
    ctx()->delay_fn([src = src_, snk = snk_, n] { //
      src->on_request(snk.get(), n);
    });
}

void subscription::fwd_impl::dispose() {
  if (src_) {
    ctx()->delay_fn([src = src_, snk = snk_] { //
      src->on_cancel(snk.get());
    });
    src_.reset();
    snk_.reset();
  }
}

namespace {

class trivial_subscription : public subscription::impl_base {
public:
  bool disposed() const noexcept override {
    return disposed_;
  }

  void request(size_t) override {
    // nop
  }

  void dispose() override {
    disposed_ = true;
  }

private:
  bool disposed_ = false;
};

} // namespace

subscription subscription::make_trivial() {
  return subscription{make_counted<trivial_subscription>()};
}

} // namespace caf::flow
