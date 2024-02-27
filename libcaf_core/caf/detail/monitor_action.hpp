// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/system_messages.hpp"

#include <mutex>

namespace caf::detail {

/// A thread safe single shot action encapsulating a function and a function
/// argument @ref down_msg.
template <class F>
class monitor_action : public detail::atomic_ref_counted, public action::impl {
public:
  monitor_action(F fn)
    : state_(action::state::scheduled),
      f_(function_wrapper{std::move(fn), down_msg{}}) {
    // nop
  }

  ~monitor_action() {
    std::lock_guard guard_{mut};
    if (state_ == action::state::scheduled)
      f_.~function_wrapper();
  }

  void dispose() override {
    std::lock_guard guard_{mut};
    if (state_ == action::state::scheduled) {
      state_ = action::state::disposed;
      f_.~function_wrapper();
      return;
    }
  }

  bool disposed() const noexcept override {
    std::lock_guard guard_{mut};
    return state_ == action::state::disposed
           || state_ == action::state::deferred_dispose;
  }

  action::state current_state() const noexcept override {
    return state_;
  }

  void run() override {
    // We can only run a scheduled action.
    std::lock_guard guard_{mut};
    if (state_ == action::state::scheduled) {
      state_ = action::state::disposed;
      f_();
      f_.~function_wrapper();
    }
  }

  void arg(down_msg err) {
    std::lock_guard guard_{mut};
    if (state_ == action::state::scheduled)
      f_.arg = err;
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const monitor_action* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const monitor_action* ptr) noexcept {
    ptr->deref();
  }

private:
  struct function_wrapper {
    F f_;
    down_msg arg;
    auto operator()() {
      return std::move(f_)(std::move(arg));
    }
  };
  action::state state_;
  mutable std::mutex mut;
  union {
    function_wrapper f_;
  };
};

} // namespace caf::detail
