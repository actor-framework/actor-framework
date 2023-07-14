// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/latch.hpp"

namespace caf::detail {

namespace {

using guard_type = std::unique_lock<std::mutex>;

} // namespace

void latch::count_down_and_wait() {
  guard_type guard{mtx_};
  if (--count_ == 0) {
    cv_.notify_all();
  } else {
    do {
      cv_.wait(guard);
    } while (count_ > 0);
  }
}

void latch::wait() {
  guard_type guard{mtx_};
  while (count_ > 0)
    cv_.wait(guard);
}

void latch::count_down() {
  guard_type guard{mtx_};
  if (--count_ == 0)
    cv_.notify_all();
}

bool latch::is_ready() const noexcept {
  guard_type guard{mtx_};
  return count_ == 0;
}

} // namespace caf::detail
