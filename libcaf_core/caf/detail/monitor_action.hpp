// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/action.hpp"
#include "caf/actor_control_block.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"

#include <mutex>

namespace caf::detail {

class CAF_CORE_EXPORT abstract_monitor_action : public action::impl {
public:
  abstract_monitor_action(weak_actor_ptr observer, weak_actor_ptr observed)
    : observer_(std::move(observer)), observed_(std::move(observed)) {
    // nop
  }

  ~abstract_monitor_action() noexcept override;

  virtual bool set_reason(error value) = 0;

  void ref() const noexcept final;

  void deref() const noexcept final;

  const weak_actor_ptr& observer() const noexcept {
    return observer_;
  }

  const weak_actor_ptr& observed() const noexcept {
    return observed_;
  }

protected:
  void on_dispose();

private:
  mutable detail::atomic_ref_count ref_count_;
  weak_actor_ptr observer_;
  weak_actor_ptr observed_;
};

using abstract_monitor_action_ptr = intrusive_ptr<abstract_monitor_action>;

/// A thread safe single shot action encapsulating a function and a function
/// argument @ref error.
template <class F>
class monitor_action : public abstract_monitor_action {
public:
  using super = abstract_monitor_action;

  explicit monitor_action(weak_actor_ptr observer, weak_actor_ptr observed,
                          F fn)
    : super(std::move(observer), std::move(observed)),
      state_(action::state::scheduled),
      f_(function_wrapper{std::move(fn), error{}}) {
    // nop
  }

  ~monitor_action() noexcept override {
    std::lock_guard guard{mtx_};
    if (state_ == action::state::scheduled) {
      f_.~function_wrapper();
    }
  }

  void dispose() override {
    bool call_on_dispose = false;
    {
      std::lock_guard guard{mtx_};
      if (state_ == action::state::scheduled) {
        state_ = action::state::disposed;
        f_.~function_wrapper();
        call_on_dispose = true;
      }
    }
    if (call_on_dispose) {
      this->on_dispose();
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

  void resume(scheduler*, uint64_t event_id) override {
    if (event_id == resumable::dispose_event_id) {
      dispose();
      return;
    }
    // We can only run a scheduled action.
    std::unique_lock guard{mtx_};
    if (state_ == action::state::scheduled) {
      state_ = action::state::disposed;
      guard.unlock();
      f_();
      f_.~function_wrapper();
    }
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
