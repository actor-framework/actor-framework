// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/ref_counted.hpp"

#include <cstddef>
#include <type_traits>

namespace caf::flow {

/// Controls the flow of items from publishers to subscribers.
class CAF_CORE_EXPORT subscription {
public:
  // -- nested types -----------------------------------------------------------

  /// Internal interface of a `subscription`.
  class CAF_CORE_EXPORT impl : public coordinated, public disposable::impl {
  public:
    using handle_type = subscription;

    ~impl() override;

    /// Signals demand for `n` more items.
    virtual void request(size_t n) = 0;

    friend void intrusive_ptr_add_ref(const impl* ptr) noexcept {
      ptr->ref_disposable();
    }

    friend void intrusive_ptr_release(const impl* ptr) noexcept {
      ptr->deref_disposable();
    }
  };

  /// Simple base type for all subscription implementations that implements the
  /// reference counting member functions.
  class CAF_CORE_EXPORT impl_base : public detail::plain_ref_counted,
                                    public impl {
  public:
    void ref_disposable() const noexcept final;

    void deref_disposable() const noexcept final;

    void ref_coordinated() const noexcept final;

    void deref_coordinated() const noexcept final;
  };

  /// Describes a listener to the subscription that will receive an event
  /// whenever the observer calls `request` or `cancel`.
  class CAF_CORE_EXPORT listener : public coordinated {
  public:
    virtual ~listener();

    virtual void on_request(coordinated* sink, size_t n) = 0;

    virtual void on_cancel(coordinated* sink) = 0;
  };

  /// Default implementation for subscriptions that forward `request` and
  /// `cancel` to a @ref listener.
  class CAF_CORE_EXPORT fwd_impl final : public impl_base {
  public:
    fwd_impl(coordinator* parent, listener* src, coordinated* snk)
      : parent_(parent), src_(src), snk_(snk) {
      // nop
    }

    bool disposed() const noexcept override;

    void request(size_t n) override;

    void dispose() override;

    coordinator* parent() const noexcept override {
      return parent_;
    }

    /// Creates a new subscription object.
    /// @param parent The owner of @p src and @p snk.
    /// @param src The @ref observable that emits items.
    /// @param snk the @ref observer that consumes items.
    /// @returns an instance of @ref fwd_impl in a @ref subscription handle.
    template <class Observable, class Observer>
    static subscription
    make(coordinator* parent, Observable* src, Observer* snk) {
      static_assert(std::is_base_of_v<listener, Observable>);
      static_assert(std::is_base_of_v<coordinated, Observer>);
      static_assert(std::is_same_v<typename Observable::output_type,
                                   typename Observer::input_type>);
      auto ptr = parent->template add_child<fwd_impl>(src, snk);
      return subscription{std::move(ptr)};
    }

    /// Like @ref make but without any type checking.
    static subscription make_unsafe(coordinator* parent, listener* src,
                                    coordinated* snk) {
      intrusive_ptr<impl> ptr{new fwd_impl(parent, src, snk), false};
      return subscription{std::move(ptr)};
    }

  private:
    coordinator* parent_;
    intrusive_ptr<listener> src_;
    intrusive_ptr<coordinated> snk_;
  };

  class CAF_CORE_EXPORT trivial_impl final : public subscription::impl_base {
  public:
    explicit trivial_impl(coordinator* parent) : parent_(parent) {
      // nop
    }

    coordinator* parent() const noexcept override;

    bool disposed() const noexcept override;

    void request(size_t) override;

    void dispose() override;

  private:
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
  void dispose() {
    if (pimpl_) {
      // Defend against impl::dispose() indirectly calling member functions on
      // this object again.
      auto ptr = intrusive_ptr<impl>{pimpl_.release(), false};
      auto* parent = ptr->parent();
      ptr->dispose();
      parent->release_later(ptr);
    }
  }

  /// @copydoc impl::request
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

  disposable as_disposable() const& noexcept {
    return disposable{pimpl_};
  }

  disposable as_disposable() && noexcept {
    return disposable{std::move(pimpl_)};
  }

  bool disposed() const noexcept {
    return !pimpl_ || pimpl_->disposed();
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
