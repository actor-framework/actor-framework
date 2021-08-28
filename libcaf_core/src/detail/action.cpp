// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/action.hpp"

#include "caf/logger.hpp"

namespace caf::detail {

action::impl::impl() : state_(action::state::scheduled) {
  // nop
}

void action::impl::dispose() {
  state_ = state::disposed;
}

bool action::impl::disposed() const noexcept {
  return state_.load() == state::disposed;
}

void action::impl::ref_disposable() const noexcept {
  ref();
}

void action::impl::deref_disposable() const noexcept {
  deref();
}

action::state action::impl::reschedule() {
  auto expected = state::invoked;
  if (state_.compare_exchange_strong(expected, state::scheduled))
    return state::scheduled;
  else
    return expected;
}

void action::run() {
  CAF_LOG_TRACE("");
  CAF_ASSERT(pimpl_ != nullptr);
  pimpl_->run();
}

} // namespace caf::detail
