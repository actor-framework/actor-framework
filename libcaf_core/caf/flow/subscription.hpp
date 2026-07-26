// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/assert.hpp"
#include "caf/detail/atomic_ref_count.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>

namespace caf::flow {

/// Controls the flow of items from publishers to subscribers.
class CAF_CORE_EXPORT subscription {
public:
  // -- nested types -----------------------------------------------------------

  /// Internal interface of a `subscription`.
  class CAF_CORE_EXPORT impl : public coordinated {
  public:
    using handle_type = subscription;

    ~impl() override;

    /// Signals that the observer is no longer interested in receiving items.
    /// Only the observer may call this member function. The difference between
    /// `cancel` and `dispose` is that the latter will call `on_complete` on the
    /// observer if it has not been called yet. Furthermore, `dispose` has to
    /// assume that it has been called from outside of the event loop and thus
    /// usually schedules an event to clean up the subscription. In contrast,
    /// `cancel` can assume that it has been called from within the event loop
    /// and thus can clean up the subscription immediately.
    virtual void cancel() = 0;

    /// Signals demand for `n` more items.
    virtual void request(size_t n) = 0;
  };

  /// Base type for subscriptions that also implement the disposable interface.
  class CAF_CORE_EXPORT impl_base : public impl, public disposable_impl {
  public:
    void ref() const noexcept override = 0; // disambiguation

    void deref() const noexcept override = 0; // disambiguation

    void dispose() final;

    void cancel() final;

  private:
    /// Called either from an event to safely dispose the subscription or from
    /// `cancel` directly.
    /// @param from_external Whether the call originates from outside of the
    ///                      event loop. When `true`, the implementation shall
    ///                      call `on_error` on the observer with error code
    ///                      `sec::disposed`. Otherwise, the implementation
    ///                      can safely assume that the subscriber has invoked
    ///                      this call and thus the implementation can simply
    ///                      drop its reference to the observer.
    virtual void do_dispose(bool from_external) = 0;
  };

  class CAF_CORE_EXPORT trivial_impl final : public subscription::impl_base {
  public:
    explicit trivial_impl(coordinator* parent) : parent_(parent) {
      // nop
    }

    void ref() const noexcept final;

    void deref() const noexcept final;

    coordinator* parent() const noexcept override;

    bool disposed() const noexcept override;

    void request(size_t) override;

  private:
    void do_dispose(bool from_external) override;

    mutable detail::atomic_ref_count ref_count_;
    coordinator* parent_;
    bool disposed_ = false;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit subscription(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  subscription() noexcept = default;
  subscription(subscription&&) noexcept = default;
  subscription(const subscription&) noexcept = default;
  subscription& operator=(subscription&&) noexcept = default;
  subscription& operator=(const subscription&) noexcept = default;

  // -- mutators ---------------------------------------------------------------

  /// Resets this handle but releases the reference count after the current
  /// coordinator cycle.
  /// @post `!valid()`
  void release_later() {
    if (pimpl_) {
      auto* parent = pimpl_->parent();
      parent->release_later(pimpl_);
      CAF_ASSERT(pimpl_ == nullptr);
    }
  }

  // -- demand signaling -------------------------------------------------------

  /// Causes the publisher to stop producing items for the subscriber. Any
  /// in-flight items may still get dispatched.
  /// @post `!valid()`
  void cancel() {
    if (pimpl_) {
      // Defend against impl::cancel() indirectly calling member functions on
      // this object again.
      auto handle = intrusive_ptr<impl>{pimpl_.release(), adopt_ref};
      auto* parent = handle->parent();
      handle->cancel();
      parent->release_later(handle);
    }
  }

  /// Signals demand for @p n more items.
  /// @pre `valid()`
  void request(size_t n) {
    pimpl_->request(n);
  }

  // -- properties -------------------------------------------------------------

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  impl* ptr() noexcept {
    return pimpl_.get();
  }

  const impl* ptr() const noexcept {
    return pimpl_.get();
  }

  intrusive_ptr<impl> as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  intrusive_ptr<impl>&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  // -- swapping ---------------------------------------------------------------

  void swap(subscription& other) noexcept {
    pimpl_.swap(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

/// @ref subscription
using subscription_impl = subscription::impl;

} // namespace caf::flow
