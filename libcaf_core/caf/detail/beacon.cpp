// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/beacon.hpp"

namespace caf::detail {

void beacon::ref_disposable() const noexcept {
  ref();
}

void beacon::deref_disposable() const noexcept {
  deref();
}

void beacon::dispose() {
  std::unique_lock guard{mtx_};
  if (state_ == state::waiting) {
    state_ = state::disposed;
    cv_.notify_all();
  }
}

bool beacon::disposed() const noexcept {
  std::unique_lock guard{mtx_};
  return state_ == state::disposed;
}

action::state beacon::current_state() const noexcept {
  std::unique_lock guard{mtx_};
  switch (state_) {
    default:
      return action::state::scheduled;
    case state::disposed:
      return action::state::disposed;
  }
}

resumable::resume_result beacon::resume(execution_unit*, size_t) {
  std::unique_lock guard{mtx_};
  state_ = state::lit;
  cv_.notify_all();
  return resumable::done;
}

} // namespace caf::detail
