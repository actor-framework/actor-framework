// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/flow/backpressure_overflow_strategy.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"

#include <deque>
#include <utility>

namespace caf::flow::op {

template <class T>
class on_backpressure_buffer_sub : public subscription::impl_base,
                                   public observer_impl<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  on_backpressure_buffer_sub(coordinator* parent, observer<T> out,
                             size_t buffer_size,
                             backpressure_overflow_strategy strategy)
    : parent_(parent),
      out_(std::move(out)),
      buffer_size_(buffer_size),
      strategy_(strategy) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !out_;
  }

  void request(size_t new_demand) override {
    if (new_demand == 0)
      return;
    demand_ += new_demand;
    if (demand_ == new_demand && !buffer_.empty()) {
      parent_->delay_fn([strong_this = intrusive_ptr{this}] { //
        strong_this->on_request();
      });
    }
  }

  // -- implementation of observer_impl ----------------------------------------

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void on_subscribe(subscription sub) override {
    if (sub_) {
      sub.cancel();
      return;
    }
    sub_ = std::move(sub);
    sub_.request(buffer_size_);
  }

  void on_next(const T& item) override {
    if (!out_)
      return;
    if (demand_ > 0 && buffer_.empty()) {
      --demand_;
      out_.on_next(item);
      if (sub_)
        sub_.request(1);
      return;
    }
    if (buffer_.size() == buffer_size_) {
      switch (strategy_) {
        case backpressure_overflow_strategy::drop_newest:
          sub_.request(1);
          break;
        case backpressure_overflow_strategy::drop_oldest:
          buffer_.pop_front();
          buffer_.push_back(item);
          sub_.request(1);
          break;
        default: // backpressure_overflow_strategy::fail
          sub_.cancel();
          buffer_.clear();
          out_.on_error(make_error(sec::backpressure_overflow));
      }
      return;
    }
    buffer_.push_back(item);
    sub_.request(1);
  }

  void on_complete() override {
    if (!out_ || src_error_)
      return;
    src_error_ = error{};
    sub_.release_later();
    if (buffer_.empty())
      out_.on_complete();
  }

  void on_error(const error& what) override {
    if (!out_ || src_error_)
      return;
    src_error_ = what;
    sub_.release_later();
    if (buffer_.empty())
      out_.on_error(what);
  }

private:
  void do_dispose(bool from_external) override {
    if (!out_)
      return;
    sub_.cancel();
    if (from_external)
      out_.on_error(make_error(sec::disposed));
    else
      out_.release_later();
  }

  void on_request() {
    while (out_ && demand_ > 0 && !buffer_.empty()) {
      --demand_;
      if (sub_)
        sub_.request(1);
      out_.on_next(buffer_.front());
      buffer_.pop_front();
    }
    if (out_ && src_error_) {
      CAF_ASSERT(!sub_);
      if (*src_error_)
        out_.on_error(*src_error_);
      else
        out_.on_complete();
    }
  }

  /// Stores the context (coordinator) that runs this flow.
  coordinator* parent_;

  /// Stores a handle to the subscribed observer.
  observer<T> out_;

  subscription sub_;

  size_t buffer_size_ = 0;

  size_t demand_ = 0;

  backpressure_overflow_strategy strategy_;

  /// Stores whether the input observable has signaled on_complete or on_error.
  /// A default-constructed error represents on_complete.
  std::optional<error> src_error_;

  std::deque<T> buffer_;
};

/// An observable that on_backpressure_buffer calls any callbacks on its
/// subscribers.
template <class T>
class on_backpressure_buffer : public hot<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  // -- constructors, destructors, and assignment operators --------------------

  on_backpressure_buffer(coordinator* parent, observable<T> decorated,
                         size_t buffer_size,
                         backpressure_overflow_strategy strategy)
    : super(parent),
      decorated_(std::move(decorated)),
      buffer_size_(buffer_size),
      strategy_(strategy) {
    // nop
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    CAF_ASSERT(out.valid());
    using sub_t = on_backpressure_buffer_sub<T>;
    auto ptr = super::parent_->add_child(std::in_place_type<sub_t>, out,
                                         buffer_size_, strategy_);
    out.on_subscribe(subscription{ptr});
    decorated_.subscribe(ptr->as_observer());
    return disposable{ptr->as_disposable()};
  }

private:
  observable<T> decorated_;
  size_t buffer_size_;
  backpressure_overflow_strategy strategy_;
};

} // namespace caf::flow::op
