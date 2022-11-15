// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/mcast.hpp"

#include <limits>

namespace caf::flow::op {

/// Publishes the items from a single operator to multiple subscribers.
template <class T>
class publish : public mcast<T>, public observer_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = mcast<T>;

  using state_type = typename super::state_type;

  using src_ptr = intrusive_ptr<base<T>>;

  // -- constructors, destructors, and assignment operators --------------------

  publish(coordinator* ctx, src_ptr src) : super(ctx), source_(std::move(src)) {
    // nop
  }

  // -- ref counting (and disambiguation due to multiple base types) -----------

  void ref_coordinated() const noexcept override {
    this->ref();
  }

  void deref_coordinated() const noexcept override {
    this->deref();
  }

  friend void intrusive_ptr_add_ref(const publish* ptr) noexcept {
    ptr->ref_coordinated();
  }

  friend void intrusive_ptr_release(const publish* ptr) noexcept {
    ptr->deref_coordinated();
  }

  // -- implementation of conn<T> ----------------------------------------------

  disposable subscribe(observer<T> out) override {
    auto result = super::subscribe(std::move(out));
    if (!connected_ && super::observer_count() == auto_connect_threshold_) {
      // Note: reset to 1 since the threshold only applies to the first connect.
      auto_connect_threshold_ = 1;
      connect();
    }
    return result;
  }

  disposable connect() {
    CAF_ASSERT(source_);
    CAF_ASSERT(!connected_);
    connected_ = true;
    return source_->subscribe(observer<T>{this});
  }

  // -- properties -------------------------------------------------------------

  void auto_connect_threshold(size_t new_value) {
    auto_connect_threshold_ = new_value;
  }

  void auto_disconnect(bool new_value) {
    auto_disconnect_ = new_value;
    ;
  }

  bool connected() const noexcept {
    return connected_;
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_next(const T& item) override {
    --in_flight_;
    this->push_all(item);
  }

  void on_complete() override {
    this->close();
  }

  void on_error(const error& what) override {
    this->abort(what);
  }

  void on_subscribe(subscription in) override {
    if (!in_) {
      in_ = in;
      in_flight_ = max_buf_size_;
      in_.request(max_buf_size_);
    } else {
      in.dispose();
    }
  }

protected:
  void try_request_more() {
    if (in_ && this->has_observers()) {
      if (auto buf_size = this->max_buffered() + in_flight_;
          max_buf_size_ > buf_size) {
        auto demand = max_buf_size_ - buf_size;
        in_flight_ += demand;
        in_.request(demand);
      }
    }
  }

private:
  void on_dispose(state_type&) override {
    try_request_more();
    if (auto_disconnect_ && connected_ && super::observer_count() == 0) {
      in_.dispose();
      in_ = nullptr;
      connected_ = false;
    }
  }

  void on_consumed_some(state_type&) override {
    try_request_more();
  }

  size_t in_flight_ = 0;
  size_t max_buf_size_ = defaults::flow::buffer_size;
  subscription in_;
  src_ptr source_;
  bool connected_ = false;
  size_t auto_connect_threshold_ = std::numeric_limits<size_t>::max();
  bool auto_disconnect_ = false;
};

} // namespace caf::flow::op
