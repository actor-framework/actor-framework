// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/fwd.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/op/mcast.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/make_counted.hpp"

#include <cstdint>

namespace caf::flow {

template <class T>
class item_publisher {
public:
  using impl_ptr = intrusive_ptr<op::mcast<T>>;

  explicit item_publisher(coordinator* ctx) {
    pimpl_ = make_counted<op::mcast<T>>(ctx);
  }

  explicit item_publisher(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
    // nop
  }

  item_publisher(item_publisher&&) noexcept = default;

  item_publisher& operator=(item_publisher&&) noexcept = default;

  item_publisher(const item_publisher&) = delete;

  item_publisher& operator=(const item_publisher&) = delete;

  ~item_publisher() {
    if (pimpl_)
      pimpl_->close();
  }

  /// Pushes an item to all subscribed observers. The publisher drops the item
  /// if no subscriber exists.
  void push(const T& item) {
    pimpl_->push_all(item);
  }

  /// Pushes the items in range `[first, last)` to all subscribed observers. The
  /// publisher drops the items if no subscriber exists.
  template <class Iterator, class Sentinel>
  void push(Iterator first, Sentinel last) {
    while (first != last)
      push(*first++);
  }

  /// Pushes the items from the initializer list to all subscribed observers.
  /// The publisher drops the items if no subscriber exists.
  void push(std::initializer_list<T> items) {
    for (auto& item : items)
      push(item);
  }

  /// Closes the publisher, eventually emitting on_complete on all observers.
  void close() {
    pimpl_->close();
  }

  /// Closes the publisher, eventually emitting on_error on all observers.
  void abort(const error& reason) {
    pimpl_->abort(reason);
  }

  /// Queries how many items the publisher may emit immediately to subscribed
  /// observers.
  size_t demand() const noexcept {
    return pimpl_->min_demand();
  }

  /// Queries how many items are currently waiting in a buffer until the
  /// observer requests additional items.
  size_t buffered() const noexcept {
    return pimpl_->max_buffered();
  }

  /// Queries whether there is at least one observer subscribed to the operator.
  bool has_observers() const noexcept {
    return pimpl_->has_observers();
  }

  /// Converts the publisher to an @ref observable.
  observable<T> as_observable() const {
    return observable<T>{pimpl_};
  }

  /// Subscribes a new @ref observer to the output of the publisher.
  disposable subscribe(observer<T> out) {
    return pimpl_->subscribe(out);
  }

private:
  impl_ptr pimpl_;
};

} // namespace caf::flow
