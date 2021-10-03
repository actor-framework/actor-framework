// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/config.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"

#include <atomic>

namespace caf {

/// A functional interface similar to `std::function<void()>` with dispose
/// semantics.
class CAF_CORE_EXPORT action {
public:
  // -- member types -----------------------------------------------------------

  /// Describes the current state of an `action`.
  enum class state {
    disposed,  /// The action may no longer run.
    scheduled, /// The action is scheduled for execution.
    invoked,   /// The action fired and needs rescheduling before running again.
    waiting,   /// The action waits for reschedule but didn't run yet.
  };

  /// Describes the result of an attempted state transition.
  enum class transition {
    success,  /// Transition completed as expected.
    disposed, /// No transition since the action has been disposed.
    failure,  /// No transition since preconditions did not hold.
  };

  /// Internal interface of `action`.
  class CAF_CORE_EXPORT impl : public disposable::impl {
  public:
    virtual transition reschedule() = 0;

    virtual transition run() = 0;

    virtual state current_state() const noexcept = 0;

    friend void intrusive_ptr_add_ref(const impl* ptr) noexcept {
      ptr->ref_disposable();
    }

    friend void intrusive_ptr_release(const impl* ptr) noexcept {
      ptr->deref_disposable();
    }
  };

  using impl_ptr = intrusive_ptr<impl>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit action(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
    // nop
  }

  action() noexcept = default;

  action(action&&) noexcept = default;

  action(const action&) noexcept = default;

  action& operator=(action&&) noexcept = default;

  action& operator=(const action&) noexcept = default;

  // -- observers --------------------------------------------------------------

  [[nodiscard]] bool disposed() const {
    return pimpl_->current_state() == state::disposed;
  }

  [[nodiscard]] bool scheduled() const {
    return pimpl_->current_state() == state::scheduled;
  }

  [[nodiscard]] bool invoked() const {
    return pimpl_->current_state() == state::invoked;
  }

  // -- mutators ---------------------------------------------------------------

  /// Tries to transition from `scheduled` to `invoked`, running the body of the
  /// internal function object as a side effect on success.
  /// @return whether the transition took place.
  transition run();

  /// Cancel the action if it has not been invoked yet.
  void dispose() {
    pimpl_->dispose();
  }

  /// Tries setting the state from `invoked` back to `scheduled`.
  /// @return whether the transition took place.
  transition reschedule() {
    return pimpl_->reschedule();
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

private:
  impl_ptr pimpl_;
};

} // namespace caf
namespace caf::detail {

template <class F>
struct default_action_impl : ref_counted, action::impl {
  std::atomic<action::state> state_;
  F f_;

  default_action_impl(F fn, action::state init_state)
    : state_(init_state), f_(std::move(fn)) {
    // nop
  }

  void dispose() override {
    state_ = action::state::disposed;
  }

  bool disposed() const noexcept override {
    return state_.load() == action::state::disposed;
  }

  action::state current_state() const noexcept override {
    return state_.load();
  }

  action::transition reschedule() override {
    auto st = action::state::invoked;
    for (;;) {
      if (state_.compare_exchange_strong(st, action::state::scheduled))
        return action::transition::success;
      switch (st) {
        case action::state::invoked:
        case action::state::waiting:
          break; // Try again.
        case action::state::disposed:
          return action::transition::disposed;
        default:
          return action::transition::failure;
      }
    }
  }

  action::transition run() override {
    auto st = state_.load();
    switch (st) {
      case action::state::scheduled:
        f_();
        // No retry. If this action has been disposed while running, we stay
        // in the state 'disposed'. We assume that only one thread may try to
        // transition from scheduled to invoked, while other threads may only
        // dispose the action.
        if (state_.compare_exchange_strong(st, action::state::invoked)) {
          return action::transition::success;
        } else {
          CAF_ASSERT(st == action::state::disposed);
          return action::transition::disposed;
        }
      case action::state::disposed:
        return action::transition::disposed;
      default:
        return action::transition::failure;
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
};

} // namespace caf::detail

namespace caf {

/// Convenience function for creating @ref action objects from a function
/// object.
/// @param f The body for the action.
/// @param init_state either `action::state::scheduled` or
///                   `action::state::waiting`.
template <class F>
action make_action(F f, action::state init_state = action::state::scheduled) {
  CAF_ASSERT(init_state == action::state::scheduled
             || init_state == action::state::waiting);
  using impl_t = detail::default_action_impl<F>;
  return action{make_counted<impl_t>(std::move(f), init_state)};
}

} // namespace caf

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::action)
