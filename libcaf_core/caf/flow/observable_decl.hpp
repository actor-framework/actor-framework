// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/step.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace caf::flow {

/// Represents a potentially unbound sequence of values.
template <class T>
class observable {
public:
  /// The type of emitted items.
  using output_type = T;

  /// The pointer-to-implementation type.
  using pimpl_type = intrusive_ptr<op::base<T>>;

  explicit observable(pimpl_type pimpl) noexcept : pimpl_(std::move(pimpl)) {
    // nop
  }

  observable& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  observable() noexcept = default;
  observable(observable&&) noexcept = default;
  observable(const observable&) noexcept = default;
  observable& operator=(observable&&) noexcept = default;
  observable& operator=(const observable&) noexcept = default;

  /// @copydoc impl::subscribe
  disposable subscribe(observer<T> what) {
    if (pimpl_) {
      return pimpl_->subscribe(std::move(what));
    } else {
      what.on_error(make_error(sec::invalid_observable));
      return disposable{};
    }
  }

  /// Creates a new observer that pushes all observed items to the resource.
  disposable subscribe(async::producer_resource<T> resource);

  /// Returns a transformation that applies a step function to each input.
  template <class Step>
  transformation<Step> transform(Step step);

  /// Registers a callback for `on_next` events.
  template <class F>
  auto do_on_next(F f) {
    return transform(do_on_next_step<T, F>{std::move(f)});
  }

  /// Registers a callback for `on_complete` events.
  template <class F>
  auto do_on_complete(F f) {
    return transform(do_on_complete_step<T, F>{std::move(f)});
  }

  /// Registers a callback for `on_error` events.
  template <class F>
  auto do_on_error(F f) {
    return transform(do_on_error_step<T, F>{std::move(f)});
  }

  /// Registers a callback that runs on `on_complete` or `on_error`.
  template <class F>
  auto do_finally(F f) {
    return transform(do_finally_step<T, F>{std::move(f)});
  }

  /// Catches errors by converting them into errors instead.
  auto on_error_complete() {
    return transform(on_error_complete_step<T>{});
  }

  /// Returns a transformation that selects only the first `n` items.
  transformation<limit_step<T>> take(size_t n);

  /// Returns a transformation that selects only items that satisfy `predicate`.
  template <class Predicate>
  transformation<filter_step<Predicate>> filter(Predicate prediate);

  /// Returns a transformation that selects all value until the `predicate`
  /// returns false.
  template <class Predicate>
  transformation<take_while_step<Predicate>> take_while(Predicate prediate);

  /// Reduces the entire sequence of items to a single value. Other names for
  /// the algorithm are `accumulate` and `fold`.
  template <class Reducer>
  transformation<reduce_step<T, Reducer>> reduce(T init, Reducer reducer);

  /// Accumulates all values and emits only the final result.
  auto sum() {
    return reduce(T{}, std::plus<T>{});
  }

  /// Makes all values unique by suppressing all items that have been emitted in
  /// the past.
  transformation<distinct_step<T>> distinct();

  /// Returns a transformation that applies `f` to each input and emits the
  /// result of the function application.
  template <class F>
  transformation<map_step<F>> map(F f);

  /// Calls `on_next` for each item emitted by this observable.
  template <class OnNext>
  disposable for_each(OnNext on_next);

  /// Calls `on_next` for each item and `on_error` for each error emitted by
  /// this observable.
  template <class OnNext, class OnError>
  disposable for_each(OnNext on_next, OnError on_error);

  /// Calls `on_next` for each item, `on_error` for each error and `on_complete`
  /// for each completion signal emitted by this observable.
  template <class OnNext, class OnError, class OnComplete>
  disposable for_each(OnNext on_next, OnError on_error, OnComplete on_complete);

  /// Combines the output of multiple @ref observable objects into one by
  /// merging their outputs. May also be called without arguments if the `T` is
  /// an @ref observable.
  template <class Out = output_type, class... Inputs>
  auto merge(Inputs&&... xs);

  /// Combines the output of multiple @ref observable objects into one by
  /// concatenating their outputs. May also be called without arguments if the
  /// `T` is an @ref observable.
  template <class Out = output_type, class... Inputs>
  auto concat(Inputs&&...);

  /// Returns a transformation that emits items by merging the outputs of all
  /// observables returned by `f`.
  template <class Out = output_type, class F>
  auto flat_map(F f);

  /// Returns a transformation that emits items by concatenating the outputs of
  /// all observables returned by `f`.
  template <class Out = output_type, class F>
  auto concat_map(F f);

  /// Takes @p prefix_size elements from this observable and emits it in a tuple
  /// containing an observable for the remaining elements as the second value.
  /// The returned observable either emits a single element (the tuple) or none
  /// if this observable never produces sufficient elements for the prefix.
  /// @pre `prefix_size > 0`
  observable<cow_tuple<std::vector<T>, observable<T>>>
  prefix_and_tail(size_t prefix_size);

  /// Similar to `prefix_and_tail(1)` but passes the single element directly in
  /// the tuple instead of wrapping it in a list.
  observable<cow_tuple<T, observable<T>>> head_and_tail();

  /// Convert this observable into a @ref connectable observable.
  connectable<T> publish();

  /// Convenience alias for `publish().ref_count(subscriber_threshold)`.
  observable<T> share(size_t subscriber_threshold = 1);

  /// Transform this `observable` by applying a function object to it.
  template <class Fn>
  auto compose(Fn&& fn) & {
    return fn(*this);
  }

  /// Fn this `observable` by applying a function object to it.
  template <class Fn>
  auto compose(Fn&& fn) && {
    return fn(std::move(*this));
  }

  /// Creates an asynchronous resource that makes emitted items available in a
  /// spsc buffer.
  async::consumer_resource<T> to_resource(size_t buffer_size,
                                          size_t min_request_size);

  async::consumer_resource<T> to_resource() {
    return to_resource(defaults::flow::buffer_size, defaults::flow::min_demand);
  }

  observable observe_on(coordinator* other, size_t buffer_size,
                        size_t min_request_size);

  observable observe_on(coordinator* other) {
    return observe_on(other, defaults::flow::buffer_size,
                      defaults::flow::min_demand);
  }

  const observable& as_observable() const& noexcept {
    return std::move(*this);
  }

  observable&& as_observable() && noexcept {
    return std::move(*this);
  }

  const pimpl_type& pimpl() const noexcept {
    return pimpl_;
  }

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  void swap(observable& other) {
    pimpl_.swap(other.pimpl_);
  }

  /// @pre `valid()`
  coordinator* ctx() const {
    return pimpl_->ctx();
  }

private:
  pimpl_type pimpl_;
};

/// Convenience function for creating an @ref observable from a concrete
/// operator type.
/// @relates observable
template <class Operator, class... Ts>
observable<typename Operator::output_type>
make_observable(coordinator* ctx, Ts&&... xs) {
  using out_t = typename Operator::output_type;
  using ptr_t = intrusive_ptr<op::base<out_t>>;
  ptr_t ptr{new Operator(ctx, std::forward<Ts>(xs)...), false};
  return observable<out_t>{std::move(ptr)};
}

// Note: the definition of all member functions is in observable.hpp.

} // namespace caf::flow
