// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/flow/fwd.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/op/mcast.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstdint>

namespace caf::flow {

/// A multicaster pushes items to any number of subscribers.
template <class T>
class multicaster {
public:
  using impl_ptr = intrusive_ptr<op::mcast<T>>;

  explicit multicaster(coordinator* parent) {
    pimpl_ = parent->add_child(std::in_place_type<op::mcast<T>>);
  }

  explicit multicaster(impl_ptr ptr) noexcept : pimpl_(std::move(ptr)) {
    // nop
  }

  multicaster(multicaster&&) noexcept = default;

  multicaster& operator=(multicaster&&) noexcept = default;

  multicaster(const multicaster&) = delete;

  multicaster& operator=(const multicaster&) = delete;

  ~multicaster() {
    if (pimpl_)
      pimpl_->close();
  }

  /// Pushes an item to all subscribed observers. The publisher drops the item
  /// if no subscriber exists.
  bool push(const T& item) {
    return pimpl_->push_all(item);
  }

  /// Pushes the items in range `[first, last)` to all subscribed observers. The
  /// publisher drops the items if no subscriber exists.
  template <class Iterator, class Sentinel>
  size_t push(Iterator first, Sentinel last) {
    return std::accumulate(first, last, size_t{0},
                           [this](size_t x, const T& y) {
                             return x + static_cast<size_t>(push(y));
                           });
  }

  /// Pushes the items from the initializer list to all subscribed observers.
  /// The publisher drops the items if no subscriber exists.
  size_t push(std::initializer_list<T> items) {
    return push(items.begin(), items.end());
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

  /// @private
  op::mcast<T>& impl() {
    return *pimpl_;
  }

private:
  impl_ptr pimpl_;
};

} // namespace caf::flow
