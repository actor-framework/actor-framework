// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/allowed_unsafe_message_type.hpp"
#include "caf/config.hpp"
#include "caf/detail/atomic_ref_counted.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/make_counted.hpp"

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
    disposed,  /// The action may no longer run.
    scheduled, /// The action is scheduled for execution.
  };

  /// Internal interface of `action`.
  class CAF_CORE_EXPORT impl : public disposable::impl {
  public:
    virtual void run() = 0;

    virtual state current_state() const noexcept = 0;
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
    return pimpl_->current_state() == state::disposed;
  }

  [[nodiscard]] bool scheduled() const {
    return pimpl_->current_state() == state::scheduled;
  }

  // -- mutators ---------------------------------------------------------------

  /// Triggers the action.
  void run() {
    pimpl_->run();
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

} // namespace caf
namespace caf::detail {

template <class F, bool IsSingleShot>
struct default_action_impl : detail::atomic_ref_counted, action::impl {
  std::atomic<action::state> state_;
  F f_;

  default_action_impl(F fn)
    : state_(action::state::scheduled), f_(std::move(fn)) {
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

  void run() override {
    if (state_.load() == action::state::scheduled) {
      f_();
      if constexpr (IsSingleShot)
        state_ = action::state::disposed;
      // else: allow the action to re-register itself when needed by *not*
      //       setting the state to disposed, e.g., to implement time loops.
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
