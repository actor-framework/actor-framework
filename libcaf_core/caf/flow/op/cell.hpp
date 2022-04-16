// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/observer.hpp"
#include "caf/flow/op/hot.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/none.hpp"

#include <memory>
#include <utility>
#include <variant>
#include <vector>

namespace caf::flow::op {

/// State shared between one multicast operator and one subscribed observer.
template <class T>
struct cell_sub_state {
  std::variant<none_t, unit_t, T, error> content;
  std::vector<observer<T>> listeners;

  void set_null() {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = unit;
    std::vector<observer<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs) {
      listener.on_complete();
    }
  }

  void set_value(T item) {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = std::move(item);
    auto& ref = std::get<T>(content);
    std::vector<observer<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs) {
      listener.on_next(ref);
      listener.on_complete();
    }
  }

  void set_error(error what) {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = std::move(what);
    auto& ref = std::get<error>(content);
    std::vector<observer<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs)
      listener.on_error(ref);
  }

  void listen(observer<T> listener) {
    switch (content.index()) {
      case 1:
        listener.on_complete();
        break;
      case 2:
        listener.on_next(std::get<T>(content));
        listener.on_complete();
        break;
      case 3:
        listener.on_error(std::get<error>(content));
        break;
      default:
        listeners.emplace_back(std::move(listener));
    }
  }

  void drop(const observer<T>& listener) {
    if (auto i = std::find(listeners.begin(), listeners.end(), listener);
        i != listeners.end())
      listeners.erase(i);
  }
};

template <class T>
using cell_sub_state_ptr = std::shared_ptr<cell_sub_state<T>>;

template <class T>
class cell_sub : public subscription::impl_base {
public:
  // -- constructors, destructors, and assignment operators --------------------

  cell_sub(coordinator* ctx, cell_sub_state_ptr<T> state, observer<T> out)
    : ctx_(ctx), state_(std::move(state)), out_(std::move(out)) {
    // nop
  }

  // -- implementation of subscription -----------------------------------------

  bool disposed() const noexcept override {
    return !state_;
  }

  void dispose() override {
    if (state_) {
      state_->drop(out_);
      state_ = nullptr;
      out_ = nullptr;
    }
  }

  void request(size_t) override {
    if (!listening_) {
      listening_ = true;
      ctx_->delay_fn([state = state_, out = out_] { //
        state->listen(std::move(out));
      });
    }
  }

private:
  coordinator* ctx_;
  bool listening_ = false;
  cell_sub_state_ptr<T> state_;
  observer<T> out_;
};

// Base type for *hot* operators that multicast data to subscribed observers.
template <class T>
class cell : public hot<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = hot<T>;

  using state_type = cell_sub_state<T>;

  using state_ptr_type = cell_sub_state_ptr<T>;

  using observer_type = observer<T>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit cell(coordinator* ctx)
    : super(ctx), state_(std::make_shared<state_type>()) {
    // nop
  }

  void set_null() {
    state_->set_null();
  }

  void set_value(T item) {
    state_->set_value(std::move(item));
  }

  void set_error(error what) {
    state_->set_error(what);
  }

  disposable subscribe(observer<T> out) override {
    auto ptr = make_counted<cell_sub<T>>(super::ctx_, state_, out);
    out.on_subscribe(subscription{ptr});
    return disposable{std::move(ptr)};
  }

protected:
  cell_sub_state_ptr<T> state_;
};

} // namespace caf::flow::op
