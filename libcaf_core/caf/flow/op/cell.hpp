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

/// Interface for listening on a cell.
template <class T>
class cell_listener {
public:
  friend void intrusive_ptr_add_ref(const cell_listener* ptr) noexcept {
    ptr->ref_listener();
  }

  friend void intrusive_ptr_release(const cell_listener* ptr) noexcept {
    ptr->deref_listener();
  }

  virtual void on_next(const T& item) = 0;

  virtual void on_complete() = 0;

  virtual void on_error(const error& what) = 0;

  virtual void ref_listener() const noexcept = 0;

  virtual void deref_listener() const noexcept = 0;
};

/// Convenience alias for a reference-counting smart pointer.
template <class T>
using cell_listener_ptr = intrusive_ptr<cell_listener<T>>;

/// State shared between one multicast operator and one subscribed observer.
template <class T>
struct cell_sub_state {
  std::variant<none_t, unit_t, T, error> content;
  std::vector<cell_listener_ptr<T>> listeners;

  void set_null() {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = unit;
    std::vector<cell_listener_ptr<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs) {
      listener->on_complete();
    }
  }

  void set_value(T item) {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = std::move(item);
    auto& ref = std::get<T>(content);
    std::vector<cell_listener_ptr<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs) {
      listener->on_next(ref);
      listener->on_complete();
    }
  }

  void set_error(error what) {
    CAF_ASSERT(std::holds_alternative<none_t>(content));
    content = std::move(what);
    auto& ref = std::get<error>(content);
    std::vector<cell_listener_ptr<T>> xs;
    xs.swap(listeners);
    for (auto& listener : xs)
      listener->on_error(ref);
  }

  void listen(cell_listener_ptr<T> listener) {
    switch (content.index()) {
      case 1:
        listener->on_complete();
        break;
      case 2:
        listener->on_next(std::get<T>(content));
        listener->on_complete();
        break;
      case 3:
        listener->on_error(std::get<error>(content));
        break;
      default:
        listeners.emplace_back(std::move(listener));
    }
  }

  void drop(const cell_listener_ptr<T>& listener) {
    if (auto i = std::find(listeners.begin(), listeners.end(), listener);
        i != listeners.end())
      listeners.erase(i);
  }
};

/// Convenience alias for the state of a cell.
template <class T>
using cell_sub_state_ptr = std::shared_ptr<cell_sub_state<T>>;

/// The subscription object for interfacing an observer with the cell state.
template <class T>
class cell_sub : public subscription::impl_base, public cell_listener<T> {
public:
  // -- constructors, destructors, and assignment operators --------------------

  cell_sub(coordinator* parent, cell_sub_state_ptr<T> state, observer<T> out)
    : parent_(parent), state_(std::move(state)), out_(std::move(out)) {
    // nop
  }

  // -- disambiguation ---------------------------------------------------------

  friend void intrusive_ptr_add_ref(const cell_sub* ptr) noexcept {
    ptr->ref_disposable();
  }

  friend void intrusive_ptr_release(const cell_sub* ptr) noexcept {
    ptr->deref_disposable();
  }

  // -- implementation of subscription -----------------------------------------

  coordinator* parent() const noexcept override {
    return parent_;
  }

  bool disposed() const noexcept override {
    return !state_;
  }

  void request(size_t) override {
    if (!listening_) {
      listening_ = true;
      auto self = cell_listener_ptr<T>{this};
      parent_->delay_fn([state = state_, self]() mutable { //
        state->listen(std::move(self));
      });
    }
  }

  // -- implementation of listener<T> ------------------------------------------

  void on_next(const T& item) override {
    if (out_)
      out_.on_next(item);
  }

  void on_complete() override {
    state_ = nullptr;
    if (out_)
      out_.on_complete();
  }

  void on_error(const error& what) override {
    state_ = nullptr;
    if (out_)
      out_.on_error(what);
  }

  void ref_listener() const noexcept override {
    ref_disposable();
  }

  void deref_listener() const noexcept override {
    deref_disposable();
  }

private:
  void do_dispose(bool from_external) override {
    if (state_) {
      state_->drop(this);
      state_ = nullptr;
    }
    if (out_) {
      if (from_external)
        out_.on_complete();
      else
        out_.release_later();
    }
  }

  coordinator* parent_;
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

  explicit cell(coordinator* parent)
    : super(parent), state_(std::make_shared<state_type>()) {
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
    auto ptr = super::parent_->add_child(std::in_place_type<cell_sub<T>>,
                                         state_, out);
    out.on_subscribe(subscription{ptr});
    return disposable{std::move(ptr)};
  }

protected:
  cell_sub_state_ptr<T> state_;
};

} // namespace caf::flow::op
