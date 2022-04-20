// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/error.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/sec.hpp"

#include <utility>

namespace caf::flow::op {

/// Allows operators to subscribe to an observable immediately to force an eager
/// subscription while the observable that actually consumes the items
/// subscribes later. May only be subscribed once.
template <class T>
class prefetch : public hot<T>, public observer_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit prefetch(coordinator* ctx) : super(ctx) {
    // nop
  }

  // -- ref counting (and disambiguation due to multiple base types) -----------

  void ref_coordinated() const noexcept override {
    this->ref();
  }

  void deref_coordinated() const noexcept override {
    this->deref();
  }

  friend void intrusive_ptr_add_ref(const prefetch* ptr) noexcept {
    ptr->ref_coordinated();
  }

  friend void intrusive_ptr_release(const prefetch* ptr) noexcept {
    ptr->deref_coordinated();
  }

  // -- implementation of observable_impl<T> -----------------------------------

  disposable subscribe(observer<T> out) override {
    if (completed_) {
      if (err_)
        out.on_error(err_);
      else
        out.on_complete();
      return {};
    } else if (!out_ && sub_) {
      out_ = std::move(out);
      out_.on_subscribe(sub_);
      return sub_.as_disposable();
    } else {
      auto err = make_error(sec::invalid_observable,
                            "prefetch cannot add more than one subscriber");
      out.on_error(err);
      return {};
    }
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_next(const T& item) override {
    out_.on_next(item);
  }

  void on_complete() override {
    completed_ = true;
    if (out_) {
      out_.on_complete();
      out_ = nullptr;
      sub_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    completed_ = true;
    err_ = what;
    if (out_) {
      out_.on_error(what);
      out_ = nullptr;
      sub_ = nullptr;
    }
  }

  void on_subscribe(subscription sub) override {
    if (!sub_) {
      sub_ = sub;
    } else {
      sub.dispose();
    }
  }

  // -- convenience functions --------------------------------------------------

  static intrusive_ptr<base<T>> apply(intrusive_ptr<base<T>> src) {
    auto ptr = make_counted<prefetch>(src->ctx());
    src->subscribe(observer<T>{ptr});
    return ptr;
  }

private:
  bool completed_ = false;
  error err_;
  observer<T> out_;
  subscription sub_;
};

} // namespace caf::flow::op
