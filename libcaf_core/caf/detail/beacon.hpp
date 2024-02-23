// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/core_export.hpp"

#include <condition_variable>
#include <mutex>

namespace caf::detail {

class CAF_CORE_EXPORT beacon : public atomic_ref_counted, public action::impl {
public:
  enum class state {
    waiting,
    lit,
    disposed,
  };

  void ref_disposable() const noexcept override;

  void deref_disposable() const noexcept override;

  void dispose() override;

  bool disposed() const noexcept override;

  action::state current_state() const noexcept override;

  void run() override;

  [[nodiscard]] state wait() {
    std::unique_lock guard{mtx_};
    cv_.wait(guard, [this] { return state_ != state::waiting; });
    return state_;
  }

  template <class Duration>
  [[nodiscard]] state wait_for(Duration timeout) {
    std::unique_lock guard{mtx_};
    cv_.wait_for(guard, timeout, [this] { return state_ != state::waiting; });
    return state_;
  }

  template <class TimePoint>
  [[nodiscard]] state wait_until(TimePoint timeout) {
    std::unique_lock guard{mtx_};
    cv_.wait_until(guard, timeout, [this] { return state_ != state::waiting; });
    return state_;
  }

private:
  mutable std::mutex mtx_;
  std::condition_variable cv_;
  state state_ = state::waiting;
};

} // namespace caf::detail
