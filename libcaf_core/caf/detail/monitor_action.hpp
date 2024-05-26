// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"

#include <mutex>

namespace caf::detail {

class CAF_CORE_EXPORT abstract_monitor_action
  : public detail::atomic_ref_counted,
    public action::impl {
public:
  virtual bool set_reason(error value) = 0;

  void ref_disposable() const noexcept override;

  void deref_disposable() const noexcept override;
};

using abstract_monitor_action_ptr = intrusive_ptr<abstract_monitor_action>;

inline void intrusive_ptr_add_ref(const abstract_monitor_action* ptr) noexcept {
  ptr->ref();
}

inline void intrusive_ptr_release(const abstract_monitor_action* ptr) noexcept {
  ptr->deref();
}

/// A thread safe single shot action encapsulating a function and a function
/// argument @ref error.
template <class F>
class monitor_action : public abstract_monitor_action {
public:
  explicit monitor_action(F fn)
    : state_(action::state::scheduled),
      f_(function_wrapper{std::move(fn), error{}}) {
    // nop
  }

  ~monitor_action() {
    std::lock_guard guard{mtx_};
    if (state_ == action::state::scheduled)
      f_.~function_wrapper();
  }

  void dispose() override {
    std::lock_guard guard{mtx_};
    if (state_ == action::state::scheduled) {
      state_ = action::state::disposed;
      f_.~function_wrapper();
    }
  }

  bool disposed() const noexcept override {
    std::lock_guard guard{mtx_};
    return state_ == action::state::disposed
           || state_ == action::state::deferred_dispose;
  }

  action::state current_state() const noexcept override {
    return state_;
  }

  resume_result resume(scheduler*, size_t) override {
    // We can only run a scheduled action.
    std::lock_guard guard{mtx_};
    if (state_ == action::state::scheduled) {
      state_ = action::state::disposed;
      f_();
      f_.~function_wrapper();
    }
    return resumable::done;
  }

  bool set_reason(error value) override {
    std::lock_guard guard{mtx_};
    if (state_ == action::state::scheduled) {
      f_.arg = std::move(value);
      return true;
    }
    return false;
  }

private:
  struct function_wrapper {
    F f;
    error arg;
    auto operator()() {
      return std::move(f)(std::move(arg));
    }
  };
  mutable std::mutex mtx_;
  action::state state_;
  union {
    function_wrapper f_;
  };
};

} // namespace caf::detail
