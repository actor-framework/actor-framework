// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/fwd.hpp"
#include "caf/cow_vector.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/is_complete.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/step/fwd.hpp"
#include "caf/fwd.hpp"
#include "caf/intrusive_ptr.hpp"

#include <cstddef>
#include <tuple>
#include <utility>
#include <vector>

namespace caf::flow {

/// Represents a potentially unbound sequence of values.
template <class T>
class observable {
public:
  // -- member types -----------------------------------------------------------

  /// The type of emitted items.
  using output_type = T;

  /// The pointer-to-implementation type.
  using pimpl_type = intrusive_ptr<op::base<T>>;

  /// Type for drop-all subscribers.
  using ignore_t = decltype(std::ignore);

  // -- constructors, destructors, and assignment operators --------------------

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

  // -- subscribing ------------------------------------------------------------

  /// Subscribes a new observer to the items emitted by this observable.
  disposable subscribe(observer<T> what);

  /// Creates a new observer that pushes all observed items to the resource.
  disposable subscribe(async::producer_resource<T> resource);

  /// Subscribes a new observer to the items emitted by this observable.
  disposable subscribe(ignore_t);

  /// Calls `on_next` for each item emitted by this observable.
  template <class OnNext>
  disposable for_each(OnNext on_next);

  // -- transforming -----------------------------------------------------------

  /// Returns a transformation that applies a step function to each input.
  template <class Step>
  transformation<Step> transform(Step step);

  /// Makes all values unique by suppressing items that have been emitted in the
  /// past.
  template <class U = T>
  transformation<step::distinct<U>> distinct();

  /// Registers a callback for `on_complete` and `on_error` events.
  template <class F>
  transformation<step::do_finally<T, F>> do_finally(F f);

  /// Registers a callback for `on_complete` events.
  template <class F>
  transformation<step::do_on_complete<T, F>> do_on_complete(F f);

  /// Registers a callback for `on_error` events.
  template <class F>
  transformation<step::do_on_error<T, F>> do_on_error(F f);

  /// Registers a callback for `on_next` events.
  template <class F>
  transformation<step::do_on_next<F>> do_on_next(F f);

  /// Returns a transformation that selects only items that satisfy `predicate`.
  template <class Predicate>
  transformation<step::filter<Predicate>> filter(Predicate prediate);

  /// Returns a transformation that ignores all items and only forwards calls to
  /// `on_complete` and `on_error`.
  transformation<step::ignore_elements<T>> ignore_elements();

  /// Returns a transformation that applies `f` to each input and emits the
  /// result of the function application.
  template <class F>
  transformation<step::map<F>> map(F f);

  /// Recovers from errors by converting `on_error` to `on_complete` events.
  transformation<step::on_error_complete<T>> on_error_complete();

  /// Reduces the entire sequence of items to a single value. Other names for
  /// the algorithm are `accumulate` and `fold`.
  /// @param init The initial value for the reduction.
  /// @param reducer Binary operation function that will be applied.
  template <class Init, class Reducer>
  transformation<step::reduce<Reducer>> reduce(Init init, Reducer reducer);

  /// Returns a transformation that selects all but the first `n` items.
  transformation<step::skip<T>> skip(size_t n);

  /// Returns a transformation that discards only the last `n` items.
  transformation<step::skip_last<T>> skip_last(size_t n);

  /// Returns a transformation that selects only the first `n` items.
  transformation<step::take<T>> take(size_t n);

  /// Returns a transformation that selects only the last `n` items.
  transformation<step::take_last<T>> take_last(size_t n);

  /// Returns a transformation that selects all value until the `predicate`
  /// returns false.
  template <class Predicate>
  transformation<step::take_while<Predicate>> take_while(Predicate prediate);

  /// Accumulates all values and emits only the final result.
  auto sum() {
    return reduce(T{}, [](T x, T y) { return x + y; });
  }

  /// Collects all values and emits all values at once in a @ref cow_vector.
  auto to_vector() {
    using vector_type = cow_vector<output_type>;
    auto append = [](vector_type&& xs, const output_type& x) {
      xs.unshared().push_back(x);
      return xs;
    };
    return reduce(vector_type{}, append) //
      .filter([](const vector_type& xs) { return !xs.empty(); });
  }

  /// Emits items in buffers of size @p count.
  observable<cow_vector<T>> buffer(size_t count);

  /// Emits items in buffers of size up to @p count and forces an item at
  /// regular intervals .
  observable<cow_vector<T>> buffer(size_t count, timespan period);

  // -- combining --------------------------------------------------------------

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

  /// Creates an @ref observable that combines the emitted from all passed
  /// source observables by applying a function object.
  /// @param fn The zip function. Takes one element from each input at a time
  ///           and reduces them into a single result.
  /// @param input0 The first additional input.
  /// @param inputs Additional inputs, if any.
  template <class F, class T0, class... Ts>
  auto zip_with(F fn, T0 input0, Ts... inputs);

  // -- splitting --------------------------------------------------------------

  /// Takes @p prefix_size elements from this observable and emits it in a tuple
  /// containing an observable for the remaining elements as the second value.
  /// The returned observable either emits a single element (the tuple) or none
  /// if this observable never produces sufficient elements for the prefix.
  /// @pre `prefix_size > 0`
  observable<cow_tuple<cow_vector<T>, observable<T>>>
  prefix_and_tail(size_t prefix_size);

  /// Similar to `prefix_and_tail(1)` but passes the single element directly in
  /// the tuple instead of wrapping it in a list.
  observable<cow_tuple<T, observable<T>>> head_and_tail();

  // -- multicasting -----------------------------------------------------------

  /// Convert this observable into a @ref connectable observable.
  connectable<T> publish();

  /// Convenience alias for `publish().ref_count(subscriber_threshold)`.
  observable<T> share(size_t subscriber_threshold = 1);

  // -- composing --------------------------------------------------------------

  /// Transforms this `observable` by applying a function object to it.
  template <class Fn>
  auto compose(Fn&& fn) & {
    return fn(*this);
  }

  /// Transforms this `observable` by applying a function object to it.
  template <class Fn>
  auto compose(Fn&& fn) && {
    return fn(std::move(*this));
  }

  // -- batching ---------------------------------------------------------------

  /// Like @c buffer, but wraps the collected items into type-erased batches.
  observable<async::batch> collect_batches(timespan max_delay,
                                           size_t max_items);

  // -- observing --------------------------------------------------------------

  /// Observes items from this observable on another @ref coordinator.
  /// @warning The @p other @ref coordinator *must not* run at this point.
  observable observe_on(coordinator* other, size_t buffer_size,
                        size_t min_request_size);

  /// Observes items from this observable on another @ref coordinator.
  /// @warning The @p other @ref coordinator *must not* run at this point.
  observable observe_on(coordinator* other) {
    return observe_on(other, defaults::flow::buffer_size,
                      defaults::flow::min_demand);
  }

  // -- converting -------------------------------------------------------------

  /// Creates an asynchronous resource that makes emitted items available in an
  /// SPSC buffer.
  async::consumer_resource<T> to_resource(size_t buffer_size,
                                          size_t min_request_size);

  /// Creates an asynchronous resource that makes emitted items available in an
  /// SPSC buffer.
  async::consumer_resource<T> to_resource() {
    return to_resource(defaults::flow::buffer_size, defaults::flow::min_demand);
  }

  /// Creates a publisher that makes emitted items available asynchronously.
  async::publisher<T> to_publisher();

  const observable& as_observable() const& noexcept {
    return *this;
  }

  observable&& as_observable() && noexcept {
    return std::move(*this);
  }

  // -- properties -------------------------------------------------------------

  const pimpl_type& pimpl() const& noexcept {
    return pimpl_;
  }

  pimpl_type pimpl() && noexcept {
    return std::move(pimpl_);
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

  /// @pre `valid()`
  coordinator* ctx() const {
    return pimpl_->ctx();
  }

  // -- swapping ---------------------------------------------------------------

  void swap(observable& other) {
    pimpl_.swap(other.pimpl_);
  }

private:
  // -- member variables -------------------------------------------------------

  pimpl_type pimpl_;
};

/// Convenience function for creating an @ref observable from a concrete
/// operator type.
/// @relates observable
template <class Operator, class CoordinatorType, class... Ts>
observable<typename Operator::output_type>
make_observable(CoordinatorType* ctx, Ts&&... xs) {
  using out_t = typename Operator::output_type;
  using ptr_t = intrusive_ptr<op::base<out_t>>;
  ptr_t ptr{new Operator(ctx, std::forward<Ts>(xs)...), false};
  return observable<out_t>{std::move(ptr)};
}

// Note: the definition of all member functions is in observable.hpp.

} // namespace caf::flow
