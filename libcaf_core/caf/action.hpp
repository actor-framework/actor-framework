// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/make_counted.hpp"
#include "caf/resumable.hpp"

#include <atomic>
#include <cstddef>

namespace caf {

/// A functional interface similar to `std::function<void()>` with dispose
/// semantics.
class CAF_CORE_EXPORT action {
public:
  // -- member types -----------------------------------------------------------

  /// Describes the current state of an `action`.
  enum class state {
    scheduled,        /// The action is scheduled for execution.
    running,          /// The action is currently running in another thread.
    deferred_dispose, /// The action is currently running, and will be disposed.
    disposed,         /// The action may no longer run.
  };

  /// Internal interface of `action`.
  class CAF_CORE_EXPORT impl : public disposable::impl, public resumable {
  public:
    virtual state current_state() const noexcept = 0;

    subtype_t subtype() const noexcept final;

    void ref_resumable() const noexcept final;

    void deref_resumable() const noexcept final;
  };

  using impl_ptr = intrusive_ptr<impl>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit action(impl_ptr ptr) noexcept;

  action() noexcept = default;

  action(action&&) noexcept = default;

  action(const action&) noexcept = default;

  action& operator=(action&&) noexcept = default;

  action& operator=(const action&) noexcept = default;

  action& operator=(std::nullptr_t) noexcept {
    pimpl_ = nullptr;
    return *this;
  }

  // -- observers --------------------------------------------------------------

  [[nodiscard]] bool disposed() const {
    auto state = pimpl_->current_state();
    return state == state::disposed || state == state::deferred_dispose;
  }

  [[nodiscard]] bool scheduled() const {
    return !disposed();
  }

  // -- mutators ---------------------------------------------------------------

  /// Triggers the action.
  void run() {
    pimpl_->resume(nullptr, 0);
  }

  /// Cancel the action if it has not been invoked yet.
  void dispose() {
    pimpl_->dispose();
  }

  // -- conversion -------------------------------------------------------------

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] disposable as_disposable() && noexcept {
    return disposable{std::move(pimpl_)};
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] disposable as_disposable() const& noexcept {
    return disposable{pimpl_};
  }

  /// Returns a pointer to the implementation.
  [[nodiscard]] impl* ptr() const noexcept {
    return pimpl_.get();
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] impl_ptr&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] impl_ptr as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(pimpl_);
  }

  [[nodiscard]] bool operator!() const noexcept {
    return !pimpl_;
  }

private:
  impl_ptr pimpl_;
};

/// Checks whether two actions are equal by comparing their pointers.
inline bool operator==(const action& lhs, const action& rhs) noexcept {
  return lhs.ptr() == rhs.ptr();
}

/// Checks whether two actions are not equal by comparing their pointers.
inline bool operator!=(const action& lhs, const action& rhs) noexcept {
  return !(lhs == rhs);
}

} // namespace caf
namespace caf::detail {

template <class F, bool IsSingleShot>
class default_action_impl : public detail::atomic_ref_counted,
                            public action::impl {
public:
  default_action_impl(F fn)
    : state_(action::state::scheduled), f_(std::move(fn)) {
    // nop
  }

  ~default_action_impl() {
    // The action going out of scope can't be running or deferred dispose.
    CAF_ASSERT(state_.load() != action::state::running);
    CAF_ASSERT(state_.load() != action::state::deferred_dispose);
    if (state_.load() == action::state::scheduled)
      f_.~F();
  }

  void dispose() override {
    // Try changing the state to disposed
    for (;;) {
      if (auto expected = state_.load(); expected == action::state::scheduled) {
        if (state_.compare_exchange_weak(expected, action::state::disposed)) {
          f_.~F();
          return;
        }
      } else if (expected == action::state::running) {
        if (state_.compare_exchange_weak(expected,
                                         action::state::deferred_dispose))
          return;
      } else { // action::state::{deferred_dispose, disposed}
        return;
      }
    }
  }

  bool disposed() const noexcept override {
    auto current_state = state_.load();
    return current_state == action::state::disposed
           || current_state == action::state::deferred_dispose;
  }

  action::state current_state() const noexcept override {
    return state_.load();
  }

  void run_single_shot() {
    // We can only run a scheduled action.
    auto expected = action::state::scheduled;
    if (!state_.compare_exchange_strong(expected, action::state::disposed))
      return;
    f_();
    f_.~F();
  }

  resume_result run_multi_shot() {
    // We can only run a scheduled action.
    auto expected = action::state::scheduled;
    if (!state_.compare_exchange_strong(expected, action::state::running))
      return resumable::done;
    f_();
    // Once run, we can stay in the running state or switch to deferred dispose.
    expected = action::state::running;
    if (state_.compare_exchange_strong(expected, action::state::scheduled))
      return resumable::done;
    CAF_ASSERT(expected == action::state::deferred_dispose);
    [[maybe_unused]] auto ok
      = state_.compare_exchange_strong(expected, action::state::disposed);
    CAF_ASSERT(ok);
    f_.~F();
    return resumable::awaiting_message;
  }

  resume_result resume(scheduler*, size_t) override {
    if constexpr (IsSingleShot) {
      run_single_shot();
      return resumable::done;
    } else {
      return run_multi_shot();
    }
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  friend void intrusive_ptr_add_ref(const default_action_impl* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const default_action_impl* ptr) noexcept {
    ptr->deref();
  }

private:
  std::atomic<action::state> state_;
  union {
    F f_;
  };
};

} // namespace caf::detail

namespace caf {

/// Convenience function for creating an @ref action from a function object.
/// @param f The body for the action.
template <class F>
action make_action(F f) {
  using impl_t = detail::default_action_impl<F, false>;
  return action{make_counted<impl_t>(std::move(f))};
}

/// Convenience function for creating an @ref action from a function object.
/// @param f The body for the action.
template <class F>
action make_single_shot_action(F f) {
  using impl_t = detail::default_action_impl<F, true>;
  return action{make_counted<impl_t>(std::move(f))};
}

} // namespace caf

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::action)
