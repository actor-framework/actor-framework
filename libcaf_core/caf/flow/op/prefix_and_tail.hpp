// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/cow_tuple.hpp"
#include "caf/cow_vector.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cold.hpp"
#include "caf/flow/op/ucast.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"

#include <memory>
#include <vector>

namespace caf::flow::op {

/// @relates prefix_and_tail
template <class T>
class prefix_and_tail_sub : public detail::plain_ref_counted,
                            public observer_impl<T>,
                            public subscription_impl,
                            public ucast_sub_state_listener<T> {
public:
  // -- member types -----------------------------------------------------------

  using tuple_t = cow_tuple<cow_vector<T>, observable<T>>;

  using state_type = ucast_sub_state<T>;

  // -- constructors, destructors, and assignment operators --------------------

  prefix_and_tail_sub(coordinator* parent, observer<tuple_t> out,
                      size_t prefix_size)
    : parent_(parent), out_(std::move(out)), prefix_size_(prefix_size) {
    prefix_buf_.reserve(prefix_size);
  }

  ~prefix_and_tail_sub() {
    if (sink_) {
      sink_->state().listener = nullptr;
      sink_->close();
    }
  }

  // -- implementation of observer ---------------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  void ref_coordinated() const noexcept override {
    ref();
  }

  void deref_coordinated() const noexcept override {
    deref();
  }

  void on_next(const T& item) override {
    if (sink_) {
      // We're in tail mode: push to sink.
      CAF_ASSERT(in_flight_ > 0);
      --in_flight_;
      sink_->push(item);
    } else if (out_) {
      // We're waiting for the prefix.
      prefix_buf_.push_back(item);
      if (prefix_buf_.size() == prefix_size_) {
        // Create the sink to deliver to tail lazily and deliver the prefix.
        sink_ = parent_->add_child(std::in_place_type<ucast<T>>);
        sink_->state().listener = this;
        // Force member to be null before calling on_next / on_complete.
        auto out = std::move(out_);
        auto tup = make_cow_tuple(cow_vector<T>{std::move(prefix_buf_)},
                                  observable<T>{sink_});
        out.on_next(tup);
        out.on_complete();
      }
    }
  }

  void on_error(const error& reason) override {
    if (sink_) {
      sink_->state().listener = nullptr;
      sink_->abort(reason);
      sub_.release_later();
    } else if (out_) {
      out_.on_error(reason);
    }
  }

  void on_complete() override {
    if (sink_) {
      sink_->state().listener = nullptr;
      sink_->close();
      sub_.release_later();
    } else if (out_) {
      out_.on_complete();
    }
  }

  // -- implementation of observable -------------------------------------------

  void on_subscribe(flow::subscription sub) override {
    if (!sub_ && out_) {
      sub_ = std::move(sub);
      if (prefix_demand_ > 0 && !requested_prefix_) {
        sub_.request(prefix_size_);
        requested_prefix_ = true;
      }
    } else {
      sub.dispose();
    }
  }

  // -- implementation of disposable -------------------------------------------

  void dispose() override {
    if (disposed())
      return;
    sub_.dispose();
    if (sink_) {
      CAF_ASSERT(!out_); // Either in tail mode or in prefix mode.
      sink_ = nullptr;
      return;
    }
    CAF_ASSERT(out_);
    parent_->delay_fn([out = std::move(out_)]() mutable { out.on_complete(); });
  }

  bool disposed() const noexcept override {
    return !out_ && !sink_;
  }

  void ref_disposable() const noexcept override {
    ref();
  }

  void deref_disposable() const noexcept override {
    deref();
  }

  void request(size_t demand) override {
    // Only called by out_, never by sink_ (triggers on_sink_demand_change()).
    prefix_demand_ += demand;
    if (sub_ && !requested_prefix_) {
      sub_.request(prefix_size_);
      requested_prefix_ = true;
    }
  }

  // -- implementation of ucast_sub_state_listener -----------------------------

  void on_disposed(state_type*) override {
    dispose();
  }

  void on_demand_changed(state_type*) override {
    if (sink_ && sub_) {
      auto& st = sink_->state();
      auto pending = in_flight_ + st.buf.size();
      if (st.demand > pending) {
        auto delta = st.demand - pending;
        in_flight_ += delta;
        sub_.request(delta);
      }
    }
  }

private:
  intrusive_ptr<prefix_and_tail_sub> strong_this() {
    return {this};
  }

  /// Our scheduling context.
  coordinator* parent_;

  /// The observer for the initial prefix-and-tail tuple.
  observer<tuple_t> out_;

  /// Caches items for the prefix until we can emit them.
  std::vector<T> prefix_buf_;

  /// Allows us to push to the "tail" observable after emitting the prefix.
  ucast_ptr<T> sink_;

  /// Pulls data from the decorated observable.
  flow::subscription sub_;

  /// Stores how much items are currently in-flight while receiving the tail.
  size_t in_flight_ = 0;

  /// Stores whether we have asked the decorated observable for data yet.
  bool requested_prefix_ = false;

  /// Keeps track of demand of @p out_ while we receive the prefix.
  size_t prefix_demand_ = 0;

  /// Stores how many items we need to buffer for the prefix.
  size_t prefix_size_;
};

/// @relates prefix_and_tail_sub
template <class T>
void intrusive_ptr_add_ref(prefix_and_tail_sub<T>* ptr) {
  ptr->ref();
}

/// @relates prefix_and_tail_sub
template <class T>
void intrusive_ptr_release(prefix_and_tail_sub<T>* ptr) {
  ptr->deref();
}

/// Decorates an observable to split its output into a prefix of fixed size plus
/// an observable remainder.
template <class T>
class prefix_and_tail : public cold<cow_tuple<cow_vector<T>, observable<T>>> {
public:
  // -- member types -----------------------------------------------------------

  using tuple_t = cow_tuple<cow_vector<T>, observable<T>>;

  using super = cold<tuple_t>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit prefix_and_tail(coordinator* parent, observable<T> decorated,
                           size_t prefix_size)
    : super(parent),
      decorated_(std::move(decorated)),
      prefix_size_(prefix_size) {
    // nop
  }

  disposable subscribe(observer<tuple_t> out) override {
    using impl_t = prefix_and_tail_sub<T>;
    auto obs = super::parent_->add_child(std::in_place_type<impl_t>, out,
                                         prefix_size_);
    out.on_subscribe(subscription{obs});
    decorated_.subscribe(observer<T>{obs});
    return obs->as_disposable();
  }

private:
  observable<T> decorated_;
  size_t prefix_size_;
};

} // namespace caf::flow::op
