// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/counted_disposable.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/connectable.hpp"
#include "caf/flow/op/mcast.hpp"

namespace caf::flow::op {

/// Publishes the items from a single operator to multiple subscribers.
template <class T>
class publish : public mcast<T>,
                public observer_impl<T>,
                public connectable<T> {
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
    ptr->ref();
  }

  friend void intrusive_ptr_release(const publish* ptr) noexcept {
    ptr->deref();
  }

  // -- implementation of base<T> ----------------------------------------------

  disposable subscribe(observer<T> out) override {
    if (!source_) {
      out.on_error(make_error(sec::disposed));
      return {};
    }
    return super::subscribe(std::move(out));
  }

  // -- implementation of connectable<T> ---------------------------------------

  [[nodiscard]] disposable connect() override {
    if (!source_) {
      // We have removed the source since it called on_complete or on_error.
      // Hence, we can no longer connect to it.
      return {};
    }
    if (!conn_ || conn_->disposed()) {
      in_.cancel(); // Make sure stale state is cleaned up.
      auto sub = source_->subscribe(observer<T>{this});
      conn_ = make_counted<detail::counted_disposable>(std::move(sub));
    }
    return conn_->acquire();
  }

  bool connected() const noexcept override {
    return conn_ && !conn_->disposed();
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_next(const T& item) override {
    --in_flight_;
    if (this->push(item)) {
      if (in_ && this->has_observers()) {
        // If push returns `true`, it means that all observers have consumed
        // the item without buffering it. Hence, we know that
        // this->max_buffered() is 0 and we can request more items from the
        // source right away.
        ++in_flight_;
        in_.request(1);
      }
    }
  }

  void on_complete() override {
    source_ = nullptr;
    this->close();
  }

  void on_error(const error& what) override {
    if (what == sec::disposed) {
      // The subscription got disposed, which means this wasn't an actual error
      // from the source and we can probably connect to it again. If we have
      // already established a new connection, we can simply ignore this event.
      if (conn_ && conn_->disposed()) {
        conn_ = nullptr;
        in_.cancel();
      }
      return;
    }
    source_ = nullptr;
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

  /// The connection to the source operator (once connected).
  intrusive_ptr<detail::counted_disposable> conn_;

  /// Scheduled when on_consumed_some() is called. Having this as a member
  /// variable avoids allocating a new action object for each call.
  action try_request_more_;

  /// Guards against scheduling `try_request_more_` while it is already pending.
  bool try_request_more_pending_ = false;
};

} // namespace caf::flow::op
