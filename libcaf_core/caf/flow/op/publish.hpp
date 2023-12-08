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

  using state_ptr_type = mcast_sub_state_ptr<T>;

  using src_ptr = intrusive_ptr<base<T>>;

  // -- constructors, destructors, and assignment operators --------------------

  publish(coordinator* parent, src_ptr src,
          size_t max_buf_size = defaults::flow::buffer_size)
    : super(parent), max_buf_size_(max_buf_size), source_(std::move(src)) {
    try_request_more_ = make_action([this] { this->try_request_more(); });
  }

  ~publish() override {
    try_request_more_.dispose();
  }

  // -- ref counting (and disambiguation due to multiple base types) -----------

  coordinator* parent() const noexcept override {
    return super::parent_;
  }

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
  }

  bool connected() const noexcept {
    return connected_;
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_next(const T& item) override {
    --in_flight_;
    if (this->push_all(item)) {
      if (in_ && this->has_observers()) {
        // If push_all returns `true`, it means that all observers have consumed
        // the item without buffering it. Hence, we know that
        // this->max_buffered() is 0 and we can request more items from the
        // source right away.
        ++in_flight_;
        in_.request(1);
      }
    }
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
      in.cancel();
    }
  }

  // -- implementation of ucast_sub_state_listener -----------------------------

  void on_consumed_some(state_type*, size_t, size_t) override {
    if (!try_request_more_pending_) {
      try_request_more_pending_ = true;
      super::parent_->delay(try_request_more_);
    }
  }

protected:
  void try_request_more() {
    try_request_more_pending_ = false;
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
  void do_dispose(const state_ptr_type&, bool) override {
    try_request_more();
    if (auto_disconnect_ && connected_ && super::observer_count() == 0) {
      connected_ = false;
      in_.cancel();
    }
  }

  /// Keeps track of the number of items that have been requested but that have
  /// not yet been delivered.
  size_t in_flight_ = 0;

  /// Maximum number of items to buffer.
  size_t max_buf_size_;

  /// Our subscription for fetching items.
  subscription in_;

  /// The source operator we subscribe to lazily.
  src_ptr source_;

  /// Keeps track of whether we are connected to the source operator.
  bool connected_ = false;

  /// The number of observers that need to connect before we connect to the
  /// source operator.
  size_t auto_connect_threshold_ = std::numeric_limits<size_t>::max();

  /// Whether to disconnect from the source operator when the last observer
  /// unsubscribes.
  bool auto_disconnect_ = false;

  /// Scheduled when on_consumed_some() is called. Having this as a member
  /// variable avoids allocating a new action object for each call.
  action try_request_more_;

  /// Guards against scheduling `try_request_more_` while it is already pending.
  bool try_request_more_pending_ = false;
};

} // namespace caf::flow::op
