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

namespace caf::detail {

/// A functional interface similar to `std::function<void()>` with dispose
/// semantics.
class CAF_CORE_EXPORT action {
public:
  enum class state {
    disposed,
    scheduled,
    invoked,
  };

  class impl : public ref_counted, public disposable::impl {
  public:
    friend class action;

    impl();

    void dispose() override;

    bool disposed() const noexcept override;

    void ref_disposable() const noexcept override;

    void deref_disposable() const noexcept override;

    /// Tries setting the state from `invoked` back to `scheduled`.
    /// @return the new state.
    state reschedule();

    /// Runs the action if the state is `scheduled`.
    virtual void run() = 0;

    friend void intrusive_ptr_add_ref(const impl* ptr) noexcept {
      ptr->ref();
    }

    friend void intrusive_ptr_release(const impl* ptr) noexcept {
      ptr->deref();
    }

  protected:
    std::atomic<state> state_;
  };

  using impl_ptr = intrusive_ptr<impl>;

  action(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
    // nop
  }

  action() noexcept = default;

  action(action&&) noexcept = default;

  action(const action&) noexcept = default;

  action& operator=(action&&) noexcept = default;

  action& operator=(const action&) noexcept = default;

  /// Runs the action if it is still scheduled for execution, does nothing
  /// otherwise.
  void run();

  /// Cancel the action if it has not been invoked yet.
  void dispose() {
    pimpl_->dispose();
  }

  [[nodiscard]] bool disposed() const {
    return pimpl_->state_ == state::disposed;
  }

  [[nodiscard]] bool scheduled() const {
    return pimpl_->state_ == state::scheduled;
  }

  [[nodiscard]] bool invoked() const {
    return pimpl_->state_ == state::invoked;
  }

  /// Tries setting the state from `invoked` back to `scheduled`.
  /// @return the new state.
  state reschedule() {
    return pimpl_->reschedule();
  }

  /// Returns a pointer to the implementation.
  [[nodiscard]] impl* ptr() noexcept {
    return pimpl_.get();
  }

  /// Returns a pointer to the implementation.
  [[nodiscard]] const impl* ptr() const noexcept {
    return pimpl_.get();
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] disposable as_disposable() && noexcept {
    return disposable{std::move(pimpl_)};
  }

  /// Returns a smart pointer to the implementation.
  [[nodiscard]] disposable as_disposable() const& noexcept {
    return disposable{pimpl_};
  }

private:
  impl_ptr pimpl_;
};

template <class F>
action make_action(F f) {
  struct impl : action::impl {
    F f_;
    explicit impl(F fn) : f_(std::move(fn)) {
      // nop
    }
    void run() override {
      if (state_ == action::state::scheduled) {
        f_();
        auto expected = action::state::scheduled;
        // No retry. If this action has been disposed while running, we stay in
        // the state 'disposed'.
        state_.compare_exchange_strong(expected, action::state::invoked);
      }
    }
  };
  return action{make_counted<impl>(std::move(f))};
}

} // namespace caf::detail

CAF_ALLOW_UNSAFE_MESSAGE_TYPE(caf::detail::action)
