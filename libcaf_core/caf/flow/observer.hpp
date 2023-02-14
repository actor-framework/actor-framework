// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/batch.hpp"
#include "caf/async/producer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/comparable.hpp"
#include "caf/detail/plain_ref_counted.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/unit.hpp"

namespace caf::flow {

/// Handle to a consumer of items.
template <class T>
class observer : detail::comparable<observer<T>> {
public:
  /// Internal interface of an `observer`.
  class impl : public coordinated {
  public:
    using input_type = T;

    virtual void on_subscribe(subscription sub) = 0;

    virtual void on_next(const T& item) = 0;

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
  void on_next(const T& item) {
    pimpl_->on_next(item);
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

  void swap(observer& other) noexcept {
    pimpl_.swap(other.pimpl_);
  }

  intptr_t compare(const observer& other) const noexcept {
    return pimpl_.compare(other.pimpl_);
  }

private:
  intrusive_ptr<impl> pimpl_;
};

/// @relates observer
template <class T>
using observer_impl = typename observer<T>::impl;

/// Simple base type observer implementations that implements the reference
/// counting member functions with a plain (i.e., not thread-safe) reference
/// count.
/// @relates observer
template <class T>
class observer_impl_base : public detail::plain_ref_counted,
                           public observer_impl<T> {
public:
  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }
};

} // namespace caf::flow

namespace caf::detail {

template <class OnNextSignature>
struct on_next_trait;

template <class T>
struct on_next_trait<void(T)> {
  using value_type = T;
};

template <class T>
struct on_next_trait<void(const T&)> {
  using value_type = T;
};

template <class F>
using on_next_trait_t
  = on_next_trait<typename get_callable_trait_t<F>::fun_sig>;

template <class F>
using on_next_value_type = typename on_next_trait_t<F>::value_type;

template <class OnNext, class OnError = unit_t, class OnComplete = unit_t>
class default_observer_impl
  : public flow::observer_impl_base<on_next_value_type<OnNext>> {
public:
  static_assert(std::is_invocable_v<OnError, const error&>);

  static_assert(std::is_invocable_v<OnComplete>);

  using input_type = on_next_value_type<OnNext>;

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

  void on_next(const input_type& item) override {
    on_next_(item);
    sub_.request(1);
  }

  void on_error(const error& what) override {
    if (sub_) {
      on_error_(what);
      sub_ = nullptr;
    }
  }

  void on_complete() override {
    if (sub_) {
      on_complete_();
      sub_ = nullptr;
    }
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_) {
      sub_ = std::move(sub);
      sub_.request(defaults::flow::buffer_size);
    } else {
      sub.dispose();
    }
  }

private:
  OnNext on_next_;
  OnError on_error_;
  OnComplete on_complete_;
  flow::subscription sub_;
};

} // namespace caf::detail

namespace caf::flow {

/// Creates an observer from given callbacks.
/// @param on_next Callback for handling incoming elements.
/// @param on_error Callback for handling an error.
/// @param on_complete Callback for handling the end-of-stream event.
template <class OnNext, class OnError, class OnComplete>
auto make_observer(OnNext on_next, OnError on_error, OnComplete on_complete) {
  using impl_type = detail::default_observer_impl<OnNext, OnError, OnComplete>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next), std::move(on_error),
                                     std::move(on_complete));
  return observer<input_type>{std::move(ptr)};
}

/// Creates an observer from given callbacks.
/// @param on_next Callback for handling incoming elements.
/// @param on_error Callback for handling an error.
template <class OnNext, class OnError>
auto make_observer(OnNext on_next, OnError on_error) {
  using impl_type = detail::default_observer_impl<OnNext, OnError>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next), std::move(on_error));
  return observer<input_type>{std::move(ptr)};
}

/// Creates an observer from given callbacks.
/// @param on_next Callback for handling incoming elements.
template <class OnNext>
auto make_observer(OnNext on_next) {
  using impl_type = detail::default_observer_impl<OnNext>;
  using input_type = typename impl_type::input_type;
  auto ptr = make_counted<impl_type>(std::move(on_next));
  return observer<input_type>{std::move(ptr)};
}

/// Creates an observer from a smart pointer to a custom object that implements
/// `on_next`, `on_error` and `on_complete` as member functions.
/// @param ptr Smart pointer to a custom object.
template <class SmartPointer>
auto make_observer_from_ptr(SmartPointer ptr) {
  using obj_t = std::remove_reference_t<decltype(*ptr)>;
  using on_next_fn = decltype(&obj_t::on_next);
  using value_type = typename detail::on_next_trait_t<on_next_fn>::value_type;
  return make_observer([ptr](const value_type& x) { ptr->on_next(x); },
                       [ptr](const error& what) { ptr->on_error(what); },
                       [ptr] { ptr->on_complete(); });
}

// -- writing observed values to an async buffer -------------------------------

/// Writes observed values to a bounded buffer.
template <class Buffer>
class buffer_writer_impl : public detail::atomic_ref_counted,
                           public observer_impl<typename Buffer::value_type>,
                           public async::producer {
public:
  // -- member types -----------------------------------------------------------

  using buffer_ptr = intrusive_ptr<Buffer>;

  using value_type = typename Buffer::value_type;

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

  // -- intrusive_ptr interface ------------------------------------------------

  friend void intrusive_ptr_add_ref(const buffer_writer_impl* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const buffer_writer_impl* ptr) noexcept {
    ptr->deref();
  }

  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }

  void ref_producer() const noexcept final {
    this->ref();
  }

  void deref_producer() const noexcept final {
    this->deref();
  }

  // -- implementation of observer<T>::impl ------------------------------------

  void on_next(const value_type& item) override {
    CAF_LOG_TRACE(CAF_ARG(item));
    if (buf_)
      buf_->push(item);
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
      sub.dispose();
    }
  }

  // -- implementation of async::producer: must be thread-safe -----------------

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

private:
  void on_demand(size_t n) {
    CAF_LOG_TRACE(CAF_ARG(n));
    if (sub_)
      sub_.request(n);
  }

  void on_cancel() {
    CAF_LOG_TRACE("");
    if (sub_) {
      sub_.dispose();
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

// -- utility observer ---------------------------------------------------------

/// Forwards all events to its parent.
template <class T, class Parent, class Token>
class forwarder : public observer_impl_base<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit forwarder(intrusive_ptr<Parent> parent, Token token)
    : parent_(std::move(parent)), token_(std::move(token)) {
    // nop
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_complete() override {
    if (parent_) {
      parent_->fwd_on_complete(token_);
      parent_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (parent_) {
      parent_->fwd_on_error(token_, what);
      parent_ = nullptr;
    }
  }

  void on_subscribe(subscription new_sub) override {
    if (parent_) {
      parent_->fwd_on_subscribe(token_, std::move(new_sub));
    } else {
      new_sub.dispose();
    }
  }

  void on_next(const T& item) override {
    if (parent_) {
      parent_->fwd_on_next(token_, item);
    }
  }

private:
  intrusive_ptr<Parent> parent_;
  Token token_;
};

} // namespace caf::flow
