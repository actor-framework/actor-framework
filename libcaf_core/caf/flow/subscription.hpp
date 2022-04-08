// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/disposable.hpp"
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
  class CAF_CORE_EXPORT impl : public disposable::impl {
  public:
    ~impl() override;

    /// Causes the publisher to stop producing items for the subscriber. Any
    /// in-flight items may still get dispatched.
    virtual void cancel() = 0;

    /// Signals demand for `n` more items.
    virtual void request(size_t n) = 0;

    void dispose() final;
  };

  /// A trivial subscription type that drops all member function calls.
  class CAF_CORE_EXPORT nop_impl final : public ref_counted, public impl {
  public:
    // -- friends --------------------------------------------------------------

    CAF_INTRUSIVE_PTR_FRIENDS(nop_impl)

    bool disposed() const noexcept override;

    void ref_disposable() const noexcept override;

    void deref_disposable() const noexcept override;

    void cancel() override;

    void request(size_t n) override;

  private:
    bool disposed_ = false;
  };

  /// Describes a listener to the subscription that will receive an event
  /// whenever the observer calls `request` or `cancel`.
  class CAF_CORE_EXPORT listener : public disposable::impl {
  public:
    virtual ~listener();

    virtual void on_request(disposable::impl* sink, size_t n) = 0;

    virtual void on_cancel(disposable::impl* sink) = 0;
  };

  /// Default implementation for subscriptions that forward `request` and
  /// `cancel` to a @ref listener.
  class default_impl final : public ref_counted, public impl {
  public:
    CAF_INTRUSIVE_PTR_FRIENDS(default_impl)

    default_impl(coordinator* ctx, listener* src, disposable::impl* snk)
      : ctx_(ctx), src_(src), snk_(snk) {
      // nop
    }

    bool disposed() const noexcept override;

    void ref_disposable() const noexcept override;

    void deref_disposable() const noexcept override;

    void request(size_t n) override;

    void cancel() override;

    auto* ctx() const noexcept {
      return ctx_;
    }

    /// Creates a new subscription object.
    /// @param ctx The owner of @p src and @p snk.
    /// @param src The @ref observable that emits items.
    /// @param snk the @ref observer that consumes items.
    /// @returns an instance of @ref default_impl in a @ref subscription handle.
    template <class Observable, class Observer>
    static subscription make(coordinator* ctx, Observable* src, Observer* snk) {
      static_assert(std::is_base_of_v<listener, Observable>);
      static_assert(std::is_base_of_v<disposable_impl, Observer>);
      static_assert(std::is_same_v<typename Observable::output_type,
                                   typename Observer::input_type>);
      intrusive_ptr<impl> ptr{new default_impl(ctx, src, snk), false};
      return subscription{std::move(ptr)};
    }

    /// Like @ref make but without any type checking.
    static subscription make_unsafe(coordinator* ctx, listener* src,
                                    disposable_impl* snk) {
      intrusive_ptr<impl> ptr{new default_impl(ctx, src, snk), false};
      return subscription{std::move(ptr)};
    }

  private:
    coordinator* ctx_;
    intrusive_ptr<listener> src_;
    intrusive_ptr<disposable_impl> snk_;
  };

  // -- constructors, destructors, and assignment operators --------------------

  explicit subscription(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  subscription& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  subscription() noexcept = default;
  subscription(subscription&&) noexcept = default;
  subscription(const subscription&) noexcept = default;
  subscription& operator=(subscription&&) noexcept = default;
  subscription& operator=(const subscription&) noexcept = default;

  // -- demand signaling -------------------------------------------------------

  /// @copydoc impl::cancel
  void cancel() {
    if (pimpl_) {
      pimpl_->cancel();
      pimpl_ = nullptr;
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

  // -- swapping ---------------------------------------------------------------

  void swap(subscription& other) noexcept {
    pimpl_.swap(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

} // namespace caf::flow
