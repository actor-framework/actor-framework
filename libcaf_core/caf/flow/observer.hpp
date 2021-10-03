// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/batch.hpp"
#include "caf/async/producer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/span.hpp"
#include "caf/unit.hpp"

namespace caf::flow {

/// Handle to a consumer of items.
template <class T>
class observer {
public:
  /// Internal interface of an `observer`.
  class impl : public disposable::impl {
  public:
    using input_type = T;

    virtual void on_subscribe(subscription sub) = 0;

    virtual void on_next(span<const T> items) = 0;

    virtual void on_complete() = 0;

    virtual void on_error(const error& what) = 0;

    observer as_observer() noexcept {
      return observer{intrusive_ptr<impl>(this)};
    }
  };

  using input_type = T;

  explicit observer(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  observer& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  observer() noexcept = default;
  observer(observer&&) noexcept = default;
  observer(const observer&) noexcept = default;
  observer& operator=(observer&&) noexcept = default;
  observer& operator=(const observer&) noexcept = default;

  disposable as_disposable() const& noexcept {
    return disposable{pimpl_};
  }

  disposable as_disposable() && noexcept {
    return disposable{std::move(pimpl_)};
  }

  /// @pre `valid()`
  void on_complete() {
    pimpl_->on_complete();
  }

  /// @pre `valid()`
  void on_error(const error& what) {
    pimpl_->on_error(what);
  }

  /// @pre `valid()`
  void on_subscribe(subscription sub) {
    pimpl_->on_subscribe(std::move(sub));
  }

  /// @pre `valid()`
  void on_batch(const async::batch& buf) {
    pimpl_->on_batch(buf);
  }

  /// @pre `valid()`
  void on_next(span<const T> items) {
    pimpl_->on_next(items);
  }

  /// Creates a new observer from `Impl`.
  template <class Impl, class... Ts>
  [[nodiscard]] static observer make(Ts&&... xs) {
    static_assert(std::is_base_of_v<impl, Impl>);
    return observer{make_counted<Impl>(std::forward<Ts>(xs)...)};
  }

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  impl* ptr() {
    return pimpl_.get();
  }

  const impl* ptr() const {
    return pimpl_.get();
  }

  const intrusive_ptr<impl>& as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  intrusive_ptr<impl>&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  void swap(observer& other) {
    pimpl_.swap(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

template <class T>
using observer_impl = typename observer<T>::impl;

// -- writing observed values to a buffer --------------------------------------

/// Writes observed values to a bounded buffer.
template <class Buffer>
class buffer_writer_impl : public ref_counted,
                           public observer_impl<typename Buffer::value_type>,
                           public async::producer {
public:
  // -- member types -----------------------------------------------------------

  using buffer_ptr = intrusive_ptr<Buffer>;

  using value_type = typename Buffer::value_type;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(buffer_writer_impl)

  // -- constructors, destructors, and assignment operators --------------------

  buffer_writer_impl(coordinator* ctx, buffer_ptr buf)
    : ctx_(ctx), buf_(std::move(buf)) {
    CAF_ASSERT(ctx_ != nullptr);
    CAF_ASSERT(buf_ != nullptr);
  }

  ~buffer_writer_impl() {
    if (buf_)
      buf_->close();
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    CAF_LOG_TRACE("");
    on_complete();
  }

  bool disposed() const noexcept override {
    return buf_ == nullptr;
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  // -- implementation of observer<T>::impl ------------------------------------

  void on_next(span<const value_type> items) override {
    CAF_LOG_TRACE(CAF_ARG(items));
    if (buf_)
      buf_->push(items);
  }

  void on_complete() override {
    CAF_LOG_TRACE("");
    if (buf_) {
      buf_->close();
      buf_ = nullptr;
      sub_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (buf_) {
      buf_->abort(what);
      buf_ = nullptr;
      sub_ = nullptr;
    }
  }

  void on_subscribe(subscription sub) override {
    CAF_LOG_TRACE("");
    if (buf_ && !sub_) {
      CAF_LOG_DEBUG("add subscription");
      sub_ = std::move(sub);
    } else {
      CAF_LOG_DEBUG("already have a subscription or buffer no longer valid");
      sub.cancel();
    }
  }

  // -- implementation of async::producer: these may get called concurrently ---

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    CAF_LOG_TRACE("");
    ctx_->schedule_fn([ptr{strong_ptr()}] {
      CAF_LOG_TRACE("");
      ptr->on_cancel();
    });
  }

  void on_consumer_demand(size_t demand) override {
    CAF_LOG_TRACE(CAF_ARG(demand));
    ctx_->schedule_fn([ptr{strong_ptr()}, demand] { //
      CAF_LOG_TRACE(CAF_ARG(demand));
      ptr->on_demand(demand);
    });
  }

  void ref_producer() const noexcept final {
    this->ref();
  }

  void deref_producer() const noexcept final {
    this->deref();
  }

private:
  void on_demand(size_t n) {
    CAF_LOG_TRACE(CAF_ARG(n));
    if (sub_)
      sub_.request(n);
  }

  void on_cancel() {
    CAF_LOG_TRACE("");
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
    }
    buf_ = nullptr;
  }

  intrusive_ptr<buffer_writer_impl> strong_ptr() {
    return {this};
  }

  coordinator_ptr ctx_;
  buffer_ptr buf_;
  subscription sub_;
};

} // namespace caf::flow

namespace caf::detail {

template <class OnNextSignature>
struct on_next_trait;

template <class T>
struct on_next_trait<void(T)> {
  using value_type = T;

  template <class F>
  static void apply(F& f, span<const T> items) {
    for (auto&& item : items)
      f(item);
  }
};

template <class T>
struct on_next_trait<void(const T&)> {
  using value_type = T;

  template <class F>
  static void apply(F& f, span<const T> items) {
    for (auto&& item : items)
      f(item);
  }
};

template <class T>
struct on_next_trait<void(span<const T>)> {
  using value_type = T;

  template <class F>
  static void apply(F& f, span<const T> items) {
    f(items);
  }
};

template <class F>
using on_next_trait_t
  = on_next_trait<typename get_callable_trait_t<F>::fun_sig>;

template <class F>
using on_next_value_type = typename on_next_trait_t<F>::value_type;

template <class OnNext, class OnError = unit_t, class OnComplete = unit_t>
class default_observer_impl
  : public ref_counted,
    public flow::observer_impl<on_next_value_type<OnNext>> {
public:
  static_assert(std::is_invocable_v<OnError, const error&>);

  static_assert(std::is_invocable_v<OnComplete>);

  using input_type = on_next_value_type<OnNext>;

  CAF_INTRUSIVE_PTR_FRIENDS(default_observer_impl)

  explicit default_observer_impl(OnNext&& on_next_fn)
    : on_next_(std::move(on_next_fn)) {
    // nop
  }

  default_observer_impl(OnNext&& on_next_fn, OnError&& on_error_fn)
    : on_next_(std::move(on_next_fn)), on_error_(std::move(on_error_fn)) {
    // nop
  }

  default_observer_impl(OnNext&& on_next_fn, OnError&& on_error_fn,
                        OnComplete&& on_complete_fn)
    : on_next_(std::move(on_next_fn)),
      on_error_(std::move(on_error_fn)),
      on_complete_(std::move(on_complete_fn)) {
    // nop
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  void on_next(span<const input_type> items) override {
    if (!completed_) {
      on_next_trait_t<OnNext>::apply(on_next_, items);
      sub_.request(items.size());
    }
  }

  void on_error(const error& what) override {
    if (!completed_) {
      on_error_(what);
      sub_ = nullptr;
      completed_ = true;
    }
  }

  void on_complete() override {
    if (!completed_) {
      on_complete_();
      sub_ = nullptr;
      completed_ = true;
    }
  }

  void on_subscribe(flow::subscription sub) override {
    if (!completed_ && !sub_) {
      sub_ = std::move(sub);
      sub_.request(defaults::flow::buffer_size);
    } else {
      sub.cancel();
    }
  }

  void dispose() override {
    if (!completed_) {
      on_complete_();
      if (sub_) {
        sub_.cancel();
        sub_ = nullptr;
      }
      completed_ = true;
    }
  }

  bool disposed() const noexcept override {
    return completed_;
  }

private:
  bool completed_ = false;
  OnNext on_next_;
  OnError on_error_;
  OnComplete on_complete_;
  flow::subscription sub_;
};

} // namespace caf::detail

namespace caf::flow {

template <class OnNext, class OnError, class OnComplete>
auto make_observer(OnNext on_next, OnError on_error, OnComplete on_complete) {
  using impl_type = detail::default_observer_impl<OnNext, OnError, OnComplete>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next), std::move(on_error),
                                     std::move(on_complete));
  return observer<input_type>{std::move(ptr)};
}

template <class OnNext, class OnError>
auto make_observer(OnNext on_next, OnError on_error) {
  using impl_type = detail::default_observer_impl<OnNext, OnError>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next), std::move(on_error));
  return observer<input_type>{std::move(ptr)};
}

template <class OnNext>
auto make_observer(OnNext on_next) {
  using impl_type = detail::default_observer_impl<OnNext>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next));
  return observer<input_type>{std::move(ptr)};
}

} // namespace caf::flow
