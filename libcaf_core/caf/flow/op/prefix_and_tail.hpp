// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/cow_tuple.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/cell.hpp"
#include "caf/flow/subscription.hpp"

#include <cstdint>

namespace caf::flow::op {

template <class T>
class pipe_sub : public detail::plain_ref_counted,
                 public observer_impl<T>,
                 public subscription_impl {
public:
  // -- constructors, destructors, and assignment operators --------------------

  explicit pipe_sub(subscription in) : in_(std::move(in)) {
    // nop
  }

  void init(observer<T> out) {
    if (!completed_)
      out_ = std::move(out);
    else if (err_)
      out.on_error(err_);
    else
      out.on_complete();
  }

  // -- ref counting -----------------------------------------------------------

  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  friend void intrusive_ptr_add_ref(const pipe_sub* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const pipe_sub* ptr) noexcept {
    ptr->deref();
  }

  // -- implementation of observer_impl<Input> ---------------------------------

  void on_next(const T& item) override {
    if (out_)
      out_.on_next(item);
  }

  void on_complete() override {
    if (out_) {
      in_ = nullptr;
      out_.on_complete();
      out_ = nullptr;
      completed_ = true;
    } else if (!completed_) {
      completed_ = true;
    }
  }

  void on_error(const error& what) override {
    if (out_) {
      in_ = nullptr;
      out_.on_error(what);
      out_ = nullptr;
      completed_ = true;
      err_ = what;
    } else if (!completed_) {
      completed_ = true;
      err_ = what;
    }
  }

  void on_subscribe(subscription in) override {
    in.dispose();
  }

  // -- implementation of subscription_impl ------------------------------------

  bool disposed() const noexcept override {
    return !out_;
  }

  void dispose() override {
    if (in_) {
      in_.dispose();
      in_ = nullptr;
    }
    if (out_) {
      out_.on_complete();
      out_ = nullptr;
    }
  }

  void request(size_t n) override {
    if (in_) {
      in_.request(n);
    }
  }

private:
  subscription in_;
  observer<T> out_;

  /// Stores whether on_complete has been called. This may be the case even
  /// before init gets called.
  bool completed_ = false;

  /// Stores the error passed to on_error.
  error err_;
};

template <class T>
class pipe : public hot<T> {
public:
  using super = hot<T>;

  using sub_ptr = intrusive_ptr<pipe_sub<T>>;

  explicit pipe(coordinator* ctx, sub_ptr sub)
    : super(ctx), sub_(std::move(sub)) {
    // nop
  }

  ~pipe() {
    if (sub_)
      sub_->dispose();
  }

  disposable subscribe(observer<T> out) override {
    if (sub_) {
      sub_->init(out);
      out.on_subscribe(subscription{sub_});
      return disposable{std::move(sub_)};
    } else {
      auto err = make_error(sec::invalid_observable,
                            "pipes may only be subscribed once");
      out.on_error(err);
      return disposable{};
    }
  }

private:
  sub_ptr sub_;
};

template <class T>
class prefix_and_tail_fwd : public detail::plain_ref_counted,
                            public observer_impl<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  using output_type = cow_tuple<std::vector<T>, observable<T>>;

  using cell_ptr = intrusive_ptr<cell<output_type>>;

  prefix_and_tail_fwd(coordinator* ctx, size_t prefix_size, cell_ptr sync)
    : ctx_(ctx), prefix_size_(prefix_size), sync_(std::move(sync)) {
    // nop
  }

  // -- ref counting -----------------------------------------------------------

  void ref_coordinated() const noexcept final {
    this->ref();
  }

  void deref_coordinated() const noexcept final {
    this->deref();
  }

  friend void intrusive_ptr_add_ref(const prefix_and_tail_fwd* ptr) noexcept {
    ptr->ref();
  }

  friend void intrusive_ptr_release(const prefix_and_tail_fwd* ptr) noexcept {
    ptr->deref();
  }

  // -- implementation of observer_impl<T> -------------------------------------

  void on_next(const T& item) override {
    if (next_) {
      next_.on_next(item);
    } else if (sync_) {
      prefix_.push_back(item);
      if (prefix_.size() == prefix_size_) {
        auto fwd = make_counted<pipe_sub<T>>(std::move(in_));
        auto tmp = make_counted<pipe<T>>(ctx_, fwd);
        output_type result{std::move(prefix_), observable<T>{tmp}};
        sync_->set_value(std::move(result));
        sync_ = nullptr;
        next_ = observer<T>{fwd};
      }
    }
  }

  void on_complete() override {
    if (next_) {
      next_.on_complete();
      next_ = nullptr;
    } else if (sync_) {
      sync_->set_null();
      sync_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (next_) {
      next_.on_error(what);
      next_ = nullptr;
    } else if (sync_) {
      sync_->set_error(what);
      sync_ = nullptr;
    }
  }

  void on_subscribe(subscription in) override {
    in_ = std::move(in);
    in_.request(prefix_size_);
  }

private:
  coordinator* ctx_;
  size_t prefix_size_ = 0;
  subscription in_;
  std::vector<T> prefix_;
  observer<T> next_;
  cell_ptr sync_;
};

template <class T>
class prefix_and_tail : public cold<cow_tuple<std::vector<T>, observable<T>>> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = cow_tuple<std::vector<T>, observable<T>>;

  using super = cold<output_type>;

  using observer_type = observer<T>;

  // -- constructors, destructors, and assignment operators --------------------

  prefix_and_tail(coordinator* ctx, intrusive_ptr<base<T>> decorated,
                  size_t prefix_size)
    : super(ctx), decorated_(std::move(decorated)), prefix_size_(prefix_size) {
    // nop
  }

  disposable subscribe(observer<output_type> out) override {
    // The cell acts as a sort of handshake between the dispatcher and the
    // observer. After producing the (prefix, tail) pair, the dispatcher goes on
    // to forward items from the decorated observable to the tail.
    auto sync = make_counted<cell<output_type>>(super::ctx_);
    auto fwd = make_counted<prefix_and_tail_fwd<T>>(super::ctx_, prefix_size_,
                                                    sync);
    std::vector<disposable> result;
    result.reserve(2);
    result.emplace_back(sync->subscribe(std::move(out)));
    result.emplace_back(decorated_->subscribe(observer<T>{std::move(fwd)}));
    return disposable::make_composite(std::move(result));
  }

private:
  intrusive_ptr<base<T>> decorated_;
  size_t prefix_size_;
};

} // namespace caf::flow::op
