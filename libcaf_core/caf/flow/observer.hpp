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
#include "caf/log/core.hpp"
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

    using handle_type = observer;

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

  observer() noexcept = default;
  observer(observer&&) noexcept = default;
  observer(const observer&) noexcept = default;
  observer& operator=(observer&&) noexcept = default;
  observer& operator=(const observer&) noexcept = default;

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

  // -- callbacks for the subscription -----------------------------------------

  /// @pre `valid()`
  /// @post `!valid()`
  void on_complete() {
    CAF_ASSERT(pimpl_ != nullptr);
    // Defend against impl::on_complete() indirectly calling member functions on
    // this object again.
    auto ptr = intrusive_ptr<impl>{pimpl_.release(), false};
    auto* parent = ptr->parent();
    ptr->on_complete();
    parent->release_later(ptr);
  }

  /// @pre `valid()`
  /// @post `!valid()`
  void on_error(const error& what) {
    CAF_ASSERT(pimpl_ != nullptr);
    // Defend against impl::on_error() indirectly calling member functions on
    // this object again.
    auto ptr = intrusive_ptr<impl>{pimpl_.release(), false};
    auto* parent = ptr->parent();
    ptr->on_error(what);
    parent->release_later(ptr);
  }

  // -- properties -------------------------------------------------------------

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

  impl* ptr() noexcept {
    return pimpl_.get();
  }

  const impl* ptr() const noexcept {
    return pimpl_.get();
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

template <class T, class OnNext, class OnError = unit_t,
          class OnComplete = unit_t>
class default_observer_impl : public flow::observer_impl_base<T> {
public:
  static_assert(std::is_invocable_v<OnNext, const T&>);

  static_assert(std::is_invocable_v<OnError, const error&>);

  static_assert(std::is_invocable_v<OnComplete>);

  using input_type = T;

  default_observer_impl(flow::coordinator* parent, OnNext&& on_next_fn)
    : parent_(parent), on_next_(std::move(on_next_fn)) {
    // nop
  }

  default_observer_impl(flow::coordinator* parent, OnNext&& on_next_fn,
                        OnError&& on_error_fn)
    : parent_(parent),
      on_next_(std::move(on_next_fn)),
      on_error_(std::move(on_error_fn)) {
    // nop
  }

  default_observer_impl(flow::coordinator* parent, OnNext&& on_next_fn,
                        OnError&& on_error_fn, OnComplete&& on_complete_fn)
    : parent_(parent),
      on_next_(std::move(on_next_fn)),
      on_error_(std::move(on_error_fn)),
      on_complete_(std::move(on_complete_fn)) {
    // nop
  }

  flow::coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_next(const input_type& item) override {
    on_next_(item);
    sub_.request(1);
  }

  void on_error(const error& what) override {
    if (sub_) {
      on_error_(what);
      sub_.release_later();
    }
  }

  void on_complete() override {
    if (sub_) {
      on_complete_();
      sub_.release_later();
    }
  }

  void on_subscribe(flow::subscription sub) override {
    if (!sub_) {
      sub_ = std::move(sub);
      sub_.request(defaults::flow::buffer_size);
    } else {
      sub.cancel();
    }
  }

private:
  flow::coordinator* parent_;
  OnNext on_next_;
  OnError on_error_;
  OnComplete on_complete_;
  flow::subscription sub_;
};

} // namespace caf::detail

namespace caf::flow {

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

  buffer_writer_impl(coordinator* parent) : parent_(parent) {
    CAF_ASSERT(parent_ != nullptr);
  }

  ~buffer_writer_impl() {
    if (buf_)
      buf_->close();
  }

  void init(buffer_ptr buf) {
    // This step is a bit subtle. Basically, buf->set_producer might throw, in
    // which case we must not set buf_ to avoid closing a buffer that we don't
    // actually own.
    CAF_ASSERT(buf != nullptr);
    buf->set_producer(this);
    buf_ = std::move(buf);
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

  coordinator* parent() const noexcept override {
    return parent_.get();
  }

  void on_next(const value_type& item) override {
    auto lg = log::core::trace("item = {}",
                               const_cast<const value_type*>(&item));
    if (buf_)
      buf_->push(item);
  }

  void on_complete() override {
    auto lg = log::core::trace("");
    if (buf_) {
      buf_->close();
      buf_ = nullptr;
      sub_.release_later();
    }
  }

  void on_error(const error& what) override {
    auto lg = log::core::trace("what = {}", what);
    if (buf_) {
      buf_->abort(what);
      buf_ = nullptr;
      sub_.release_later();
    }
  }

  void on_subscribe(subscription sub) override {
    auto lg = log::core::trace("");
    if (buf_ && !sub_) {
      log::core::debug("add subscription");
      sub_ = std::move(sub);
    } else {
      log::core::debug("already have a subscription or buffer no longer valid");
      sub.cancel();
    }
  }

  // -- implementation of async::producer: must be thread-safe -----------------

  void on_consumer_ready() override {
    // nop
  }

  void on_consumer_cancel() override {
    auto lg = log::core::trace("");
    parent_->schedule_fn([ptr{strong_ptr()}] {
      auto lg = log::core::trace("");
      ptr->on_cancel();
    });
  }

  void on_consumer_demand(size_t demand) override {
    auto lg = log::core::trace("demand = {}", demand);
    parent_->schedule_fn([ptr{strong_ptr()}, demand] { //
      auto lg = log::core::trace("demand = {}", demand);
      ptr->on_demand(demand);
    });
  }

private:
  void on_demand(size_t n) {
    auto lg = log::core::trace("n = {}", n);
    if (sub_)
      sub_.request(n);
  }

  void on_cancel() {
    auto lg = log::core::trace("");
    if (sub_) {
      sub_.cancel();
      sub_.release_later();
    }
    buf_ = nullptr;
  }

  intrusive_ptr<buffer_writer_impl> strong_ptr() {
    return {this};
  }

  coordinator_ptr parent_;
  buffer_ptr buf_;
  subscription sub_;
};

// -- utility observer ---------------------------------------------------------

/// Forwards all events to its parent.
template <class T, class Target, class Token>
class forwarder : public observer_impl_base<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit forwarder(coordinator* parent, intrusive_ptr<Target> target,
                     Token token)
    : parent_(parent), target_(std::move(target)), token_(std::move(token)) {
    // nop
  }

  // -- implementation of observer_impl<T> -------------------------------------

  flow::coordinator* parent() const noexcept override {
    return parent_;
  }

  void on_complete() override {
    if (target_) {
      target_->fwd_on_complete(token_);
      target_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (target_) {
      target_->fwd_on_error(token_, what);
      target_ = nullptr;
    }
  }

  void on_subscribe(subscription new_sub) override {
    if (target_) {
      target_->fwd_on_subscribe(token_, std::move(new_sub));
    } else {
      new_sub.cancel();
    }
  }

  void on_next(const T& item) override {
    if (target_) {
      target_->fwd_on_next(token_, item);
    }
  }

private:
  coordinator* parent_;
  intrusive_ptr<Target> target_;
  Token token_;
};

} // namespace caf::flow
