// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/consumer.hpp"
#include "caf/async/producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinated.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observable_state.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/op/concat.hpp"
#include "caf/flow/op/from_resource.hpp"
#include "caf/flow/op/from_steps.hpp"
#include "caf/flow/op/merge.hpp"
#include "caf/flow/op/prefix_and_tail.hpp"
#include "caf/flow/op/publish.hpp"
#include "caf/flow/step.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"

#include <cstddef>
#include <functional>
#include <numeric>
#include <type_traits>
#include <vector>

namespace caf::flow {

/// Base type for classes that represent a definition of an `observable` which
/// has not yet been converted to an actual `observable`.
template <class T>
class observable_def {
public:
  virtual ~observable_def() = default;

  template <class OnNext>
  auto for_each(OnNext on_next) && {
    return alloc().for_each(std::move(on_next));
  }

  template <class OnNext, class OnError>
  auto for_each(OnNext on_next, OnError on_error) && {
    return alloc().for_each(std::move(on_next), std::move(on_error));
  }

  template <class OnNext, class OnError, class OnComplete>
  auto for_each(OnNext on_next, OnError on_error, OnComplete on_complete) && {
    return alloc().for_each(std::move(on_next), std::move(on_error),
                            std::move(on_complete));
  }

  template <class... Inputs>
  auto merge(Inputs&&... xs) && {
    return alloc().as_observable().merge(std::forward<Inputs>(xs)...);
  }

  template <class... Inputs>
  auto concat(Inputs&&... xs) && {
    return alloc().as_observable().concat(std::forward<Inputs>(xs)...);
  }

  template <class F>
  auto flat_map(F f) && {
    return alloc().flat_map(std::move(f));
  }

  template <class F>
  auto concat_map(F f) && {
    return alloc().concat_map(std::move(f));
  }

  /// @copydoc observable::publish
  auto publish() && {
    return alloc().publish();
  }

  /// @copydoc observable::share
  auto share(size_t subscriber_threshold = 1) && {
    return alloc().share(subscriber_threshold);
  }

  observable<cow_tuple<std::vector<T>, observable<T>>>
  prefix_and_tail(size_t prefix_size) && {
    return alloc().prefix_and_tail(prefix_size);
  }

  observable<cow_tuple<T, observable<T>>> head_and_tail() && {
    return alloc().head_and_tail();
  }

  disposable subscribe(observer<T> what) && {
    return alloc().subscribe(std::move(what));
  }

  disposable subscribe(async::producer_resource<T> resource) && {
    return alloc().subscribe(std::move(resource));
  }

  async::consumer_resource<T> to_resource() && {
    return alloc().to_resource();
  }

  async::consumer_resource<T> to_resource(size_t buffer_size,
                                          size_t min_request_size) && {
    return alloc().to_resource(buffer_size, min_request_size);
  }

  observable<T> observe_on(coordinator* other) && {
    return alloc().observe_on(other);
  }

  observable<T> observe_on(coordinator* other, size_t buffer_size,
                           size_t min_request_size) && {
    return alloc().observe_on(other, buffer_size, min_request_size);
  }

  virtual observable<T> as_observable() && = 0;

private:
  /// Allocates and returns an actual @ref observable.
  decltype(auto) alloc() {
    return std::move(*this).as_observable();
  }
};

// -- connectable --------------------------------------------------------------

/// Resembles a regular @ref observable, except that it does not begin emitting
/// items when it is subscribed to. Only after calling `connect` will the
/// `connectable` start to emit items.
template <class T>
class connectable {
public:
  /// The type of emitted items.
  using output_type = T;

  /// The pointer-to-implementation type.
  using pimpl_type = intrusive_ptr<op::publish<T>>;

  explicit connectable(pimpl_type pimpl) noexcept : pimpl_(std::move(pimpl)) {
    // nop
  }

  connectable& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  connectable() noexcept = default;
  connectable(connectable&&) noexcept = default;
  connectable(const connectable&) noexcept = default;
  connectable& operator=(connectable&&) noexcept = default;
  connectable& operator=(const connectable&) noexcept = default;

  /// Returns an @ref observable that automatically connects to this
  /// `connectable` when reaching `subscriber_threshold` subscriptions.
  observable<T> auto_connect(size_t subscriber_threshold = 1) & {
    auto ptr = make_counted<op::publish<T>>(ctx(), pimpl_);
    ptr->auto_connect_threshold(subscriber_threshold);
    return observable<T>{ptr};
  }

  /// Similar to the `lvalue` overload, but converts this `connectable` directly
  /// if possible, thus saving one hop on the pipeline.
  observable<T> auto_connect(size_t subscriber_threshold = 1) && {
    if (pimpl_->unique() && !pimpl_->connected()) {
      pimpl_->auto_connect_threshold(subscriber_threshold);
      return observable<T>{std::move(pimpl_)};
    } else {
      auto ptr = make_counted<op::publish<T>>(ctx(), pimpl_);
      ptr->auto_connect_threshold(subscriber_threshold);
      return observable<T>{ptr};
    }
  }

  /// Returns an @ref observable that automatically connects to this
  /// `connectable` when reaching `subscriber_threshold` subscriptions and
  /// disconnects automatically after the last subscriber canceled its
  /// subscription.
  /// @note The threshold only applies to the initial connect, not to any
  ///       re-connects.
  observable<T> ref_count(size_t subscriber_threshold = 1) & {
    auto ptr = make_counted<op::publish<T>>(ctx(), pimpl_);
    ptr->auto_connect_threshold(subscriber_threshold);
    ptr->auto_disconnect(true);
    return observable<T>{ptr};
  }

  /// Similar to the `lvalue` overload, but converts this `connectable` directly
  /// if possible, thus saving one hop on the pipeline.
  observable<T> ref_count(size_t subscriber_threshold = 1) && {
    if (pimpl_->unique() && !pimpl_->connected()) {
      pimpl_->auto_connect_threshold(subscriber_threshold);
      pimpl_->auto_disconnect(true);
      return observable<T>{std::move(pimpl_)};
    } else {
      auto ptr = make_counted<op::publish<T>>(ctx(), pimpl_);
      ptr->auto_connect_threshold(subscriber_threshold);
      ptr->auto_disconnect(true);
      return observable<T>{ptr};
    }
  }

  /// Connects to the source @ref observable, thus starting to emit items.
  disposable connect() {
    return pimpl_->connect();
  }

  /// @copydoc observable::compose
  template <class Fn>
  auto compose(Fn&& fn) & {
    return fn(*this);
  }

  /// @copydoc observable::compose
  template <class Fn>
  auto compose(Fn&& fn) && {
    return fn(std::move(*this));
  }

  template <class... Ts>
  disposable subscribe(Ts&&... xs) {
    return as_observable().subscribe(std::forward<Ts>(xs)...);
  }

  observable<T> as_observable() const& {
    return observable<T>{pimpl_};
  }

  observable<T> as_observable() && {
    return observable<T>{std::move(pimpl_)};
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

  void swap(connectable& other) {
    pimpl_.swap(other.pimpl_);
  }

  /// @pre `valid()`
  coordinator* ctx() const {
    return pimpl_->ctx();
  }

private:
  pimpl_type pimpl_;
};

// -- transformation -----------------------------------------------------------

/// A special type of observer that applies a series of transformation steps to
/// its input before broadcasting the result as output.
template <class Step, class... Steps>
class transformation final
  : public observable_def<steps_output_type_t<Step, Steps...>> {
public:
  using input_type = typename Step::input_type;

  using output_type = steps_output_type_t<Step, Steps...>;

  template <class Tuple>
  transformation(observable<input_type> source, Tuple&& steps)
    : source_(std::move(source)), steps_(std::move(steps)) {
    // nop
  }

  transformation() = delete;
  transformation(const transformation&) = delete;
  transformation& operator=(const transformation&) = delete;

  transformation(transformation&&) = default;
  transformation& operator=(transformation&&) = default;

  /// @copydoc observable::transform
  template <class NewStep>
  transformation<Step, Steps..., NewStep> transform(NewStep step) && {
    return {std::move(source_),
            std::tuple_cat(std::move(steps_),
                           std::make_tuple(std::move(step)))};
  }

  /// @copydoc observable::compose
  template <class Fn>
  auto compose(Fn&& fn) && {
    return fn(std::move(*this));
  }

  auto take(size_t n) && {
    return std::move(*this).transform(limit_step<output_type>{n});
  }

  template <class Predicate>
  auto filter(Predicate predicate) && {
    return std::move(*this).transform(
      filter_step<Predicate>{std::move(predicate)});
  }

  template <class Predicate>
  auto take_while(Predicate predicate) && {
    return std::move(*this).transform(
      take_while_step<Predicate>{std::move(predicate)});
  }

  template <class Reducer>
  auto reduce(output_type init, Reducer reducer) && {
    return std::move(*this).transform(
      reduce_step<output_type, Reducer>{init, reducer});
  }

  auto sum() && {
    return std::move(*this).reduce(output_type{}, std::plus<output_type>{});
  }

  auto distinct() && {
    return std::move(*this).transform(distinct_step<output_type>{});
  }

  template <class F>
  auto map(F f) && {
    return std::move(*this).transform(map_step<F>{std::move(f)});
  }

  template <class F>
  auto do_on_next(F f) && {
    return std::move(*this) //
      .transform(do_on_next_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_on_complete(F f) && {
    return std::move(*this) //
      .transform(do_on_complete_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_on_error(F f) && {
    return std::move(*this) //
      .transform(do_on_error_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_finally(F f) && {
    return std::move(*this) //
      .transform(do_finally_step<output_type, F>{std::move(f)});
  }

  auto on_error_complete() {
    return std::move(*this) //
      .transform(on_error_complete_step<output_type>{});
  }

  observable<output_type> as_observable() && {
    using impl_t = op::from_steps<input_type, Step, Steps...>;
    return make_observable<impl_t>(source_.ctx(), source_.pimpl(),
                                   std::move(steps_));
  }

private:
  observable<input_type> source_;
  std::tuple<Step, Steps...> steps_;
};

// -- observable::transform ----------------------------------------------------

template <class T>
template <class Step>
transformation<Step> observable<T>::transform(Step step) {
  static_assert(std::is_same_v<typename Step::input_type, T>,
                "step object does not match the input type");
  return {*this, std::forward_as_tuple(std::move(step))};
}

// -- observable::take ---------------------------------------------------------

template <class T>
transformation<limit_step<T>> observable<T>::take(size_t n) {
  return {*this, std::forward_as_tuple(limit_step<T>{n})};
}

// -- observable::filter -------------------------------------------------------

template <class T>
template <class Predicate>
transformation<filter_step<Predicate>>
observable<T>::filter(Predicate predicate) {
  using step_type = filter_step<Predicate>;
  static_assert(std::is_same_v<typename step_type::input_type, T>,
                "predicate does not match the input type");
  return {*this, std::forward_as_tuple(step_type{std::move(predicate)})};
}

// -- observable::take_while ---------------------------------------------------

template <class T>
template <class Predicate>
transformation<take_while_step<Predicate>>
observable<T>::take_while(Predicate predicate) {
  using step_type = take_while_step<Predicate>;
  static_assert(std::is_same_v<typename step_type::input_type, T>,
                "predicate does not match the input type");
  return {*this, std::forward_as_tuple(step_type{std::move(predicate)})};
}

// -- observable::sum ----------------------------------------------------------

template <class T>
template <class Reducer>
transformation<reduce_step<T, Reducer>>
observable<T>::reduce(T init, Reducer reducer) {
  return {*this, reduce_step<T, Reducer>{init, reducer}};
}

// -- observable::distinct -----------------------------------------------------

template <class T>
transformation<distinct_step<T>> observable<T>::distinct() {
  return {*this, distinct_step<T>{}};
}

// -- observable::map ----------------------------------------------------------

template <class T>
template <class F>
transformation<map_step<F>> observable<T>::map(F f) {
  using step_type = map_step<F>;
  static_assert(std::is_same_v<typename step_type::input_type, T>,
                "map function does not match the input type");
  return {*this, std::forward_as_tuple(step_type{std::move(f)})};
}

// -- observable::for_each -----------------------------------------------------

template <class T>
template <class OnNext>
disposable observable<T>::for_each(OnNext on_next) {
  auto obs = make_observer(std::move(on_next));
  return subscribe(std::move(obs));
}

template <class T>
template <class OnNext, class OnError>
disposable observable<T>::for_each(OnNext on_next, OnError on_error) {
  auto obs = make_observer(std::move(on_next), std::move(on_error));
  return subscribe(std::move(obs));
}

template <class T>
template <class OnNext, class OnError, class OnComplete>
disposable observable<T>::for_each(OnNext on_next, OnError on_error,
                                   OnComplete on_complete) {
  auto obs = make_observer(std::move(on_next), std::move(on_error),
                           std::move(on_complete));
  return subscribe(std::move(obs));
}

// -- observable::merge --------------------------------------------------------

template <class T>
template <class Out, class... Inputs>
auto observable<T>::merge(Inputs&&... xs) {
  if constexpr (is_observable_v<Out>) {
    using value_t = output_type_t<Out>;
    using impl_t = op::merge<value_t>;
    return make_observable<impl_t>(ctx(), *this, std::forward<Inputs>(xs)...);
  } else {
    static_assert(
      sizeof...(Inputs) > 0,
      "merge without arguments expects this observable to emit observables");
    using impl_t = op::merge<Out>;
    return make_observable<impl_t>(ctx(), *this, std::forward<Inputs>(xs)...);
  }
}

// -- observable::flat_map -----------------------------------------------------

template <class T>
template <class Out, class F>
auto observable<T>::flat_map(F f) {
  using res_t = decltype(f(std::declval<const Out&>()));
  if constexpr (is_observable_v<res_t>) {
    return map([fn = std::move(f)](const Out& x) mutable {
             return fn(x).as_observable();
           })
      .merge();
  } else if constexpr (detail::is_optional_v<res_t>) {
    return map([fn = std::move(f)](const Out& x) mutable { return fn(x); })
      .filter([](const res_t& x) { return x.has_value(); })
      .map([](const res_t& x) { return *x; });
  } else {
    // Here, we dispatch to concat() instead of merging the containers. Merged
    // output is probably not what anyone would expect and since the values are
    // all available immediately, there is no good reason to mess up the emitted
    // order of values.
    static_assert(detail::is_iterable_v<res_t>);
    return map([cptr = ctx(), fn = std::move(f)](const Out& x) mutable {
             return cptr->make_observable().from_container(fn(x));
           })
      .concat();
  }
}

// -- observable::concat -------------------------------------------------------

template <class T>
template <class Out, class... Inputs>
auto observable<T>::concat(Inputs&&... xs) {
  if constexpr (is_observable_v<Out>) {
    using value_t = output_type_t<Out>;
    using impl_t = op::concat<value_t>;
    return make_observable<impl_t>(ctx(), *this, std::forward<Inputs>(xs)...);
  } else {
    static_assert(
      sizeof...(Inputs) > 0,
      "merge without arguments expects this observable to emit observables");
    using impl_t = op::concat<Out>;
    return make_observable<impl_t>(ctx(), *this, std::forward<Inputs>(xs)...);
  }
}

// -- observable::concat_map ---------------------------------------------------

template <class T>
template <class Out, class F>
auto observable<T>::concat_map(F f) {
  using res_t = decltype(f(std::declval<const Out&>()));
  if constexpr (is_observable_v<res_t>) {
    return map([fn = std::move(f)](const Out& x) mutable {
             return fn(x).as_observable();
           })
      .concat();
  } else if constexpr (detail::is_optional_v<res_t>) {
    return map([fn = std::move(f)](const Out& x) mutable { return fn(x); })
      .filter([](const res_t& x) { return x.has_value(); })
      .map([](const res_t& x) { return *x; });
  } else {
    static_assert(detail::is_iterable_v<res_t>);
    return map([cptr = ctx(), fn = std::move(f)](const Out& x) mutable {
             return cptr->make_observable().from_container(fn(x));
           })
      .concat();
  }
}

// -- observable::prefix_and_tail ----------------------------------------------

template <class T>
observable<cow_tuple<std::vector<T>, observable<T>>>
observable<T>::prefix_and_tail(size_t prefix_size) {
  using impl_t = op::prefix_and_tail<T>;
  return make_observable<impl_t>(ctx(), pimpl_, prefix_size);
}

// -- observable::prefix_and_tail ----------------------------------------------

template <class T>
observable<cow_tuple<T, observable<T>>> observable<T>::head_and_tail() {
  using tuple_t = cow_tuple<std::vector<T>, observable<T>>;
  return prefix_and_tail(1)
    .map([](const tuple_t& tup) {
      auto& [prefix, tail] = tup.data();
      CAF_ASSERT(prefix.size() == 1);
      return make_cow_tuple(prefix.front(), tail);
    })
    .as_observable();
}

// -- observable::publish ------------------------------------------------------

template <class T>
connectable<T> observable<T>::publish() {
  return connectable<T>{make_counted<op::publish<T>>(ctx(), pimpl_)};
}

// -- observable::share --------------------------------------------------------

template <class T>
observable<T> observable<T>::share(size_t subscriber_threshold) {
  return publish().ref_count(subscriber_threshold);
}

// -- observable::to_resource --------------------------------------------------

/// Reads from an observable buffer and emits the consumed items.
/// @note Only supports a single observer.
template <class T>
async::consumer_resource<T>
observable<T>::to_resource(size_t buffer_size, size_t min_request_size) {
  using buffer_type = async::spsc_buffer<T>;
  auto buf = make_counted<buffer_type>(buffer_size, min_request_size);
  auto up = make_counted<buffer_writer_impl<buffer_type>>(pimpl_->ctx(), buf);
  buf->set_producer(up);
  subscribe(up->as_observer());
  return async::consumer_resource<T>{std::move(buf)};
}

// -- observable::observe_on ---------------------------------------------------

template <class T>
observable<T> observable<T>::observe_on(coordinator* other, size_t buffer_size,
                                        size_t min_request_size) {
  auto [pull, push] = async::make_spsc_buffer_resource<T>(buffer_size,
                                                          min_request_size);
  subscribe(push);
  return make_observable<op::from_resource<T>>(other, std::move(pull));
}

// -- observable::subscribe ----------------------------------------------------

template <class T>
disposable observable<T>::subscribe(async::producer_resource<T> resource) {
  using buffer_type = typename async::consumer_resource<T>::buffer_type;
  using adapter_type = buffer_writer_impl<buffer_type>;
  if (auto buf = resource.try_open()) {
    CAF_LOG_DEBUG("subscribe producer resource to flow");
    auto adapter = make_counted<adapter_type>(pimpl_->ctx(), buf);
    buf->set_producer(adapter);
    auto obs = adapter->as_observer();
    auto sub = subscribe(std::move(obs));
    pimpl_->ctx()->watch(sub);
    return sub;
  } else {
    CAF_LOG_DEBUG("failed to open producer resource");
    return {};
  }
}

} // namespace caf::flow
