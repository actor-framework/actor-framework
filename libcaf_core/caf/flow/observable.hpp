// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/async/batch.hpp"
#include "caf/async/publisher.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/cow_vector.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/combine_latest.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable_decl.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/op/base.hpp"
#include "caf/flow/op/buffer.hpp"
#include "caf/flow/op/concat.hpp"
#include "caf/flow/op/debounce.hpp"
#include "caf/flow/op/fail.hpp"
#include "caf/flow/op/from_resource.hpp"
#include "caf/flow/op/from_steps.hpp"
#include "caf/flow/op/interval.hpp"
#include "caf/flow/op/merge.hpp"
#include "caf/flow/op/never.hpp"
#include "caf/flow/op/on_backpressure_buffer.hpp"
#include "caf/flow/op/on_error_resume_next.hpp"
#include "caf/flow/op/prefix_and_tail.hpp"
#include "caf/flow/op/publish.hpp"
#include "caf/flow/op/retry.hpp"
#include "caf/flow/op/sample.hpp"
#include "caf/flow/op/throttle_first.hpp"
#include "caf/flow/op/zip_with.hpp"
#include "caf/flow/step/all.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/log/core.hpp"
#include "caf/make_counted.hpp"
#include "caf/sec.hpp"
#include "caf/stream.hpp"
#include "caf/timespan.hpp"
#include "caf/typed_stream.hpp"

#include <cstddef>
#include <functional>
#include <numeric>
#include <type_traits>
#include <utility>

namespace caf::flow {

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
    auto ptr = make_counted<op::publish<T>>(parent(), pimpl_);
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
      auto ptr = make_counted<op::publish<T>>(parent(), pimpl_);
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
    auto ptr = make_counted<op::publish<T>>(parent(), pimpl_);
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
      auto ptr = make_counted<op::publish<T>>(parent(), pimpl_);
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
  coordinator* parent() const {
    return pimpl_->parent();
  }

private:
  pimpl_type pimpl_;
};

/// Captures the *definition* of an observable that has not materialized yet.
template <class Materializer, class... Steps>
class observable_def {
public:
  using output_type = output_type_t<Materializer, Steps...>;

  observable_def() = delete;
  observable_def(const observable_def&) = delete;
  observable_def& operator=(const observable_def&) = delete;

  observable_def(observable_def&&) = default;
  observable_def& operator=(observable_def&&) = default;

  template <size_t N = sizeof...(Steps), class = std::enable_if_t<N == 0>>
  explicit observable_def(Materializer&& materializer)
    : materializer_(std::move(materializer)) {
    // nop
  }

  observable_def(Materializer&& materializer, std::tuple<Steps...>&& steps)
    : materializer_(std::move(materializer)), steps_(std::move(steps)) {
    // nop
  }

  /// @copydoc observable::transform
  template <class NewStep>
  observable_def<Materializer, Steps..., NewStep> transform(NewStep step) && {
    return add_step(std::move(step));
  }

  /// @copydoc observable::compose
  template <class Fn>
  auto compose(Fn&& fn) && {
    return fn(std::move(*this));
  }

  /// @copydoc observable::element_at
  auto element_at(size_t n) && {
    return add_step(step::element_at<output_type>{n});
  }

  /// @copydoc observable::ignore_elements
  auto ignore_elements() && {
    return add_step(step::ignore_elements<output_type>{});
  }

  /// @copydoc observable::skip
  auto skip(size_t n) && {
    return add_step(step::skip<output_type>{n});
  }

  /// @copydoc observable::skip_last
  auto skip_last(size_t n) && {
    return add_step(step::skip_last<output_type>{n});
  }

  /// @copydoc observable::take
  auto take(size_t n) && {
    return add_step(step::take<output_type>{n});
  }

  /// @copydoc observable::first
  auto first() && {
    return add_step(step::take<output_type>{1});
  }

  /// @copydoc observable::take_last
  auto take_last(size_t n) && {
    return add_step(step::take_last<output_type>{n});
  }

  /// @copydoc observable::last
  auto last() && {
    return add_step(step::take_last<output_type>{1});
  }

  /// @copydoc observable::buffer
  auto buffer(size_t count) && {
    return materialize().buffer(count);
  }

  auto buffer(size_t count, timespan period) && {
    return materialize().buffer(count, period);
  }

  /// @copydoc observable::on_error_resume_next
  template <class Predicate, class Fallback>
  auto on_error_resume_next(Predicate&& predicate, Fallback&& fallback) && {
    return materialize().on_error_resume_next(
      std::forward<Predicate>(predicate), std::forward<Fallback>(fallback));
  }

  /// @copydoc observable::debounce
  auto debounce(timespan period) && {
    return materialize().debounce(period);
  }

  /// @copydoc observable::sample
  auto sample(timespan period) && {
    return materialize().sample(period);
  }

  /// @copydoc observable::throttle_first
  auto throttle_first(timespan period) && {
    return materialize().throttle_first(period);
  }

  /// @copydoc observable::throttle_last
  auto throttle_last(timespan period) && {
    return materialize().throttle_last(period);
  }

  template <class Predicate>
  auto filter(Predicate predicate) && {
    return add_step(step::filter<Predicate>{std::move(predicate)});
  }

  template <class Predicate>
  auto take_while(Predicate predicate) && {
    return add_step(step::take_while<Predicate>{std::move(predicate)});
  }

  template <class Init, class Reducer>
  auto reduce(Init init, Reducer reducer) && {
    using val_t = output_type;
    static_assert(std::is_invocable_r_v<Init, Reducer, Init&&, const val_t&>);
    return add_step(step::reduce<Reducer>{std::move(init), std::move(reducer)});
  }

  template <class Init, class Scanner>
  auto scan(Init init, Scanner scanner) && {
    using val_t = output_type;
    static_assert(std::is_invocable_r_v<Init, Scanner, Init&&, const val_t&>);
    return add_step(step::scan<Scanner>{std::move(init), std::move(scanner)});
  }

  /// @copydoc observable::retry
  template <class Predicate>
  auto retry(Predicate predicate) && {
    return materialize().retry(predicate);
  }

  auto sum() && {
    return std::move(*this).reduce(output_type{}, std::plus<output_type>{});
  }

  auto to_vector() && {
    using vector_type = cow_vector<output_type>;
    auto append = [](vector_type&& xs, const output_type& x) {
      xs.unshared().push_back(x);
      return xs;
    };
    return std::move(*this)
      .reduce(vector_type{}, append)
      .filter([](const vector_type& xs) { return !xs.empty(); });
  }

  auto distinct() && {
    return add_step(step::distinct<output_type>{});
  }

  template <class F>
  auto map(F f) && {
    return add_step(step::map<F>{std::move(f)});
  }

  template <class F>
  auto do_on_next(F f) && {
    return add_step(step::do_on_next<F>{std::move(f)});
  }

  template <class F>
  auto do_on_complete(F f) && {
    return add_step(step::do_on_complete<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_on_error(F f) && {
    return add_step(step::do_on_error<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_finally(F f) && {
    return add_step(step::do_finally<output_type, F>{std::move(f)});
  }

  /// @copydoc observable::on_backpressure_buffer
  auto on_backpressure_buffer(size_t buffer_size,
                              backpressure_overflow_strategy strategy
                              = backpressure_overflow_strategy::fail) && {
    return materialize().on_backpressure_buffer(buffer_size, strategy);
  }

  auto on_error_complete() && {
    return add_step(step::on_error_complete<output_type>{});
  }

  auto on_error_return_item(output_type item) && {
    return add_step(step::on_error_return_item<output_type>{std::move(item)});
  }

  template <class ErrorHandler>
  auto on_error_return(ErrorHandler error_handler) && {
    return add_step(
      step::on_error_return<ErrorHandler>{std::move(error_handler)});
  }

  /// Materializes the @ref observable.
  observable<output_type> as_observable() && {
    return materialize();
  }

  /// @copydoc observable::for_each
  template <class OnNext>
  auto for_each(OnNext on_next) && {
    return materialize().for_each(std::move(on_next));
  }

  /// @copydoc observable::for_each
  template <class OnNext, class OnError>
  auto for_each(OnNext on_next, OnError on_error) && {
    return materialize().for_each(std::move(on_next), std::move(on_error));
  }

  /// @copydoc observable::merge
  template <class... Inputs>
  auto merge(Inputs&&... xs) && {
    return materialize().merge(std::forward<Inputs>(xs)...);
  }

  /// @copydoc observable::concat
  template <class... Inputs>
  auto concat(Inputs&&... xs) && {
    return materialize().concat(std::forward<Inputs>(xs)...);
  }

  /// @copydoc observable::combine_latest
  template <class F, class... Inputs>
  auto combine_latest(F&& fn, Inputs&&... xs) && {
    static_assert(sizeof...(Inputs) > 1, "at least two inputs required");
    return combine_latest(std::forward<F>(fn), std::forward<Inputs>(xs)...);
  }

  /// @copydoc observable::start_with
  template <class Input>
  auto start_with(Input&& value) && {
    return materialize().start_with(std::forward<Input>(value));
  }

  /// @copydoc observable::flat_map
  template <class F>
  auto flat_map(F f, size_t max_concurrent) && {
    return materialize().flat_map(std::move(f), max_concurrent);
  }

  /// @copydoc observable::flat_map
  template <class F>
  auto flat_map(F f) && {
    return materialize().flat_map(std::move(f));
  }

  /// @copydoc observable::concat_map
  template <class F>
  auto concat_map(F f) && {
    return materialize().concat_map(std::move(f));
  }

  /// @copydoc observable::zip_with
  template <class F, class T0, class... Ts>
  auto zip_with(F fn, T0 input0, Ts... inputs) && {
    return materialize().zip_with(std::move(fn), std::move(input0),
                                  std::move(inputs)...);
  }

  /// @copydoc observable::publish
  auto publish() && {
    return materialize().publish();
  }

  /// @copydoc observable::share
  auto share(size_t subscriber_threshold = 1) && {
    return materialize().share(subscriber_threshold);
  }

  /// @copydoc observable::prefix_and_tail
  observable<cow_tuple<cow_vector<output_type>, observable<output_type>>>
  prefix_and_tail(size_t prefix_size) && {
    return materialize().prefix_and_tail(prefix_size);
  }

  /// @copydoc observable::head_and_tail
  observable<cow_tuple<output_type, observable<output_type>>>
  head_and_tail() && {
    return materialize().head_and_tail();
  }

  /// @copydoc observable::subscribe
  template <class Out>
  disposable subscribe(Out&& out) && {
    return materialize().subscribe(std::forward<Out>(out));
  }

  /// @copydoc observable::to_resource
  async::consumer_resource<output_type> to_resource() && {
    return materialize().to_resource();
  }

  /// @copydoc observable::to_resource
  async::consumer_resource<output_type>
  to_resource(size_t buffer_size, size_t min_request_size) && {
    return materialize().to_resource(buffer_size, min_request_size);
  }

  /// @copydoc observable::to_publisher
  async::publisher<output_type> to_publisher() && {
    return materialize().to_publisher();
  }

  /// @copydoc observable::to_stream
  template <class U = output_type>
  stream to_stream(cow_string name, timespan max_delay,
                   size_t max_items_per_batch) && {
    return materialize().template to_stream<U>(std::move(name), max_delay,
                                               max_items_per_batch);
  }

  /// @copydoc observable::to_stream
  template <class U = output_type>
  stream to_stream(std::string name, timespan max_delay,
                   size_t max_items_per_batch) && {
    return materialize().template to_stream<U>(std::move(name), max_delay,
                                               max_items_per_batch);
  }

  /// @copydoc observable::to_typed_stream
  template <class U = output_type>
  auto to_typed_stream(cow_string name, timespan max_delay,
                       size_t max_items_per_batch) && {
    return materialize().template to_typed_stream<U>(std::move(name), max_delay,
                                                     max_items_per_batch);
  }

  /// @copydoc observable::to_typed_stream
  template <class U = output_type>
  auto to_typed_stream(std::string name, timespan max_delay,
                       size_t max_items_per_batch) && {
    return materialize().template to_typed_stream<U>(std::move(name), max_delay,
                                                     max_items_per_batch);
  }

  /// @copydoc observable::observe_on
  observable<output_type> observe_on(coordinator* other) && {
    return materialize().observe_on(other);
  }

  /// @copydoc observable::observe_on
  observable<output_type> observe_on(coordinator* other, size_t buffer_size,
                                     size_t min_request_size) && {
    return materialize().observe_on(other, buffer_size, min_request_size);
  }

  bool valid() const noexcept {
    return materializer_.valid();
  }

private:
  template <class NewStep>
  observable_def<Materializer, Steps..., NewStep> add_step(NewStep step) {
    static_assert(std::is_same_v<output_type, typename NewStep::input_type>);
    return {std::move(materializer_),
            std::tuple_cat(std::move(steps_),
                           std::make_tuple(std::move(step)))};
  }

  observable<output_type> materialize() {
    return std::move(materializer_).materialize(std::move(steps_));
  }

  /// Encapsulates logic for allocating a flow operator.
  Materializer materializer_;

  /// Stores processing steps that the materializer fuses into a single flow
  /// operator.
  std::tuple<Steps...> steps_;
};

// -- transformation -----------------------------------------------------------

/// Materializes an @ref observable from a source @ref observable and one or
/// more processing steps.
template <class Input>
class transformation_materializer {
public:
  using output_type = Input;

  explicit transformation_materializer(observable<Input> source)
    : source_(std::move(source).pimpl()) {
    // nop
  }

  explicit transformation_materializer(intrusive_ptr<op::base<Input>> source)
    : source_(std::move(source)) {
    // nop
  }

  transformation_materializer() = delete;
  transformation_materializer(const transformation_materializer&) = delete;
  transformation_materializer& operator=(const transformation_materializer&)
    = delete;

  transformation_materializer(transformation_materializer&&) = default;
  transformation_materializer& operator=(transformation_materializer&&)
    = default;

  bool valid() const noexcept {
    return source_ != nullptr;
  }

  coordinator* parent() {
    return source_->parent();
  }

  template <class Step, class... Steps>
  auto materialize(std::tuple<Step, Steps...>&& steps) && {
    using impl_t = op::from_steps<Input, Step, Steps...>;
    return parent()->add_child_hdl(std::in_place_type<impl_t>, source_,
                                   std::move(steps));
  }

private:
  intrusive_ptr<op::base<Input>> source_;
};

// -- observable: subscribing --------------------------------------------------

template <class T>
disposable observable<T>::subscribe(observer<T> what) {
  CAF_ASSERT(what.valid());
  if (pimpl_)
    return pimpl_->subscribe(std::move(what));
  auto* ptr = what.ptr()->parent();
  auto sub = ptr->add_child(std::in_place_type<subscription::trivial_impl>);
  what.on_subscribe(subscription{sub});
  if (!sub->disposed())
    what.on_error(make_error(sec::invalid_observable));
  return sub->as_disposable();
}

template <class T>
disposable observable<T>::subscribe(async::producer_resource<T> resource) {
  using buffer_type = typename async::consumer_resource<T>::buffer_type;
  using adapter_type = buffer_writer_impl<buffer_type>;
  if (auto buf = resource.try_open()) {
    log::core::debug("subscribe producer resource to flow");
    auto adapter = make_counted<adapter_type>(pimpl_->parent());
    adapter->init(buf);
    auto obs = adapter->as_observer();
    auto sub = subscribe(std::move(obs));
    pimpl_->parent()->watch(sub);
    return sub;
  } else {
    log::core::debug("failed to open producer resource");
    return {};
  }
}

template <class T>
disposable observable<T>::subscribe(ignore_t) {
  return for_each([](const T&) {});
}

template <class T>
template <class OnNext>
disposable observable<T>::for_each(OnNext on_next) {
  static_assert(std::is_invocable_v<OnNext, const T&>,
                "for_each: the on_next function must accept a 'const T&'");
  using impl_type = detail::default_observer_impl<T, OnNext>;
  auto ptr = parent()->add_child(std::in_place_type<impl_type>,
                                 std::move(on_next));
  return subscribe(observer<T>{std::move(ptr)});
}

template <class T>
template <class OnNext, class OnError>
disposable observable<T>::for_each(OnNext on_next, OnError on_error) {
  static_assert(std::is_invocable_v<OnNext, const T&>,
                "for_each: the on_next function must accept a 'const T&'");
  using impl_type = detail::default_observer_impl<T, OnNext, OnError>;
  auto ptr = parent()->add_child(std::in_place_type<impl_type>,
                                 std::move(on_next), std::move(on_error));
  return subscribe(observer<T>{std::move(ptr)});
}

// -- observable: transforming -------------------------------------------------

template <class T>
template <class Step>
transformation<Step> observable<T>::transform(Step step) {
  static_assert(std::is_same_v<typename Step::input_type, T>,
                "step object does not match the input type");
  return {transformation_materializer<T>{pimpl()}, std::move(step)};
}

template <class T>
template <class U>
transformation<step::distinct<U>> observable<T>::distinct() {
  static_assert(detail::is_complete<std::hash<U>>,
                "distinct uses a hash set and thus requires std::hash<T>");
  return transform(step::distinct<U>{});
}

template <class T>
template <class F>
transformation<step::do_finally<T, F>> observable<T>::do_finally(F fn) {
  return transform(step::do_finally<T, F>{std::move(fn)});
}

template <class T>
template <class F>
transformation<step::do_on_complete<T, F>> observable<T>::do_on_complete(F fn) {
  return transform(step::do_on_complete<T, F>{std::move(fn)});
}

template <class T>
template <class F>
transformation<step::do_on_error<T, F>> observable<T>::do_on_error(F fn) {
  return transform(step::do_on_error<T, F>{std::move(fn)});
}

template <class T>
template <class F>
transformation<step::do_on_next<F>> observable<T>::do_on_next(F fn) {
  return transform(step::do_on_next<F>{std::move(fn)});
}

template <class T>
template <class Predicate>
transformation<step::filter<Predicate>>
observable<T>::filter(Predicate predicate) {
  return transform(step::filter{std::move(predicate)});
}

template <class T>
template <class F>
transformation<step::map<F>> observable<T>::map(F f) {
  return transform(step::map(std::move(f)));
}

template <class T>
transformation<step::on_error_complete<T>> observable<T>::on_error_complete() {
  return transform(step::on_error_complete<T>{});
}

template <class T>
observable<T>
observable<T>::on_backpressure_buffer(size_t buffer_size,
                                      backpressure_overflow_strategy strategy) {
  using impl_t = op::on_backpressure_buffer<T>;
  return parent()->add_child_hdl(std::in_place_type<impl_t>, *this, buffer_size,
                                 strategy);
}

template <class T>
template <class ErrorHandler>
transformation<step::on_error_return<ErrorHandler>>
observable<T>::on_error_return(ErrorHandler error_handler) {
  return transform(step::on_error_return{std::move(error_handler)});
}

template <class T>
transformation<step::on_error_return_item<T>>
observable<T>::on_error_return_item(T item) {
  return transform(step::on_error_return_item<T>{std::move(item)});
}

template <class T>
template <class Init, class Reducer>
transformation<step::reduce<Reducer>>
observable<T>::reduce(Init init, Reducer reducer) {
  static_assert(std::is_invocable_r_v<Init, Reducer, Init&&, const T&>);
  return transform(step::reduce<Reducer>{std::move(init), std::move(reducer)});
}

template <class T>
template <class Init, class Scanner>
transformation<step::scan<Scanner>>
observable<T>::scan(Init init, Scanner scanner) {
  static_assert(std::is_invocable_r_v<Init, Scanner, Init&&, const T&>);
  return transform(step::scan<Scanner>{std::move(init), std::move(scanner)});
}

template <class T>
transformation<step::element_at<T>> observable<T>::element_at(size_t n) {
  return transform(step::element_at<T>{n});
}

template <class T>
transformation<step::ignore_elements<T>> observable<T>::ignore_elements() {
  return transform(step::ignore_elements<T>{});
}

template <class T>
transformation<step::skip<T>> observable<T>::skip(size_t n) {
  return transform(step::skip<T>{n});
}

template <class T>
transformation<step::skip_last<T>> observable<T>::skip_last(size_t n) {
  return transform(step::skip_last<T>{n});
}

template <class T>
transformation<step::take<T>> observable<T>::take(size_t n) {
  return transform(step::take<T>{n});
}

template <class T>
transformation<step::take<T>> observable<T>::first() {
  return transform(step::take<T>{1});
}

template <class T>
transformation<step::take_last<T>> observable<T>::take_last(size_t n) {
  return transform(step::take_last<T>{n});
}

template <class T>
transformation<step::take_last<T>> observable<T>::last() {
  return transform(step::take_last<T>{1});
}

template <class T>
template <class Predicate>
transformation<step::take_while<Predicate>>
observable<T>::take_while(Predicate predicate) {
  return transform(step::take_while{std::move(predicate)});
}

template <class T>
observable<cow_vector<T>> observable<T>::buffer(size_t count) {
  using trait_t = op::buffer_default_trait<T>;
  auto* pptr = parent();
  auto obs = pptr->add_child_hdl(std::in_place_type<op::never<unit_t>>);
  return pptr->add_child_hdl(std::in_place_type<op::buffer<trait_t>>, count,
                             *this, std::move(obs));
}

template <class T>
observable<cow_vector<T>> observable<T>::buffer(size_t count, timespan period) {
  auto* pptr = parent();
  if (period <= timespan::zero()) {
    auto what = make_error(sec::invalid_argument,
                           "buffer operators require a positive period");
    return pptr->add_child_hdl(std::in_place_type<op::fail<cow_vector<T>>>,
                               std::move(what));
  }
  using trait_t = op::buffer_interval_trait<T>;
  using impl_t = op::buffer<trait_t>;
  auto obs = pptr->add_child_hdl(std::in_place_type<op::interval>, period,
                                 period);
  return pptr->add_child_hdl(std::in_place_type<impl_t>, count, *this,
                             std::move(obs));
}

template <class T>
observable<T> observable<T>::debounce(timespan period) {
  using impl_t = op::debounce<T>;
  auto* pptr = parent();
  return pptr->add_child_hdl(std::in_place_type<impl_t>, *this,
                             std::move(period));
}

template <class T>
observable<T> observable<T>::sample(timespan period) {
  using impl_t = op::sample<T>;
  auto* pptr = parent();
  auto obs = pptr->add_child_hdl(std::in_place_type<op::interval>, period,
                                 period);
  return pptr->add_child_hdl(std::in_place_type<impl_t>, *this, std::move(obs));
}

template <class T>
observable<T> observable<T>::throttle_first(timespan period) {
  using impl_t = op::throttle_first<T>;
  auto* pptr = parent();
  auto obs = pptr->add_child_hdl(std::in_place_type<op::interval>, period,
                                 period);
  return pptr->add_child_hdl(std::in_place_type<impl_t>, *this, std::move(obs));
}

template <class T>
observable<T> observable<T>::throttle_last(timespan period) {
  return sample(period);
}

template <class T>
template <class Predicate>
observable<T> observable<T>::retry(Predicate predicate) {
  using impl_t = op::retry<T, Predicate>;
  return parent()->add_child_hdl(std::in_place_type<impl_t>, *this, predicate);
}

template <class T>
template <class Predicate, class Fallback>
observable<T> observable<T>::on_error_resume_next(Predicate&& predicate,
                                                  Fallback&& fallback) {
  using fallback_t = std::decay_t<Fallback>;
  static_assert(is_observable_v<fallback_t>, "Fallback must be an observable");
  static_assert(std::is_same_v<typename fallback_t::output_type, T>,
                "Fallback observable must emit the same type as the source");
  using impl_t = op::on_error_resume_next<T, std::decay_t<Predicate>>;
  return parent()->add_child_hdl(
    std::in_place_type<impl_t>, *this, std::forward<Predicate>(predicate),
    std::forward<Fallback>(fallback).as_observable());
}

// -- observable: combining ----------------------------------------------------

template <class T>
template <class Out, class... Inputs>
auto observable<T>::merge_with_concurrency(size_t max_concurrent,
                                           Inputs&&... xs) {
  if constexpr (is_observable_v<Out>) {
    static_assert(sizeof...(Inputs) == 0,
                  "merge on an observable<observable<T>> allows no arguments");
    using value_t = output_type_t<Out>;
    return parent()->add_child_hdl(std::in_place_type<op::merge<value_t>>,
                                   max_concurrent, *this);
  } else {
    static_assert(sizeof...(Inputs) > 0, "no observable to merge with");
    static_assert((std::is_same_v<Out, output_type_t<std::decay_t<Inputs>>>
                   && ...),
                  "can only merge observables with the same observed type");
    return parent()->add_child_hdl(std::in_place_type<op::merge<Out>>,
                                   max_concurrent, *this,
                                   std::forward<Inputs>(xs).as_observable()...);
  }
}

template <class T>
template <class F, size_t... Indexes, class... Ts>
auto observable<T>::combine_latest_impl(
  F&& fn, std::integer_sequence<size_t, Indexes...>, Ts&&... inputs) {
  static_assert(sizeof...(Ts) > 1, "at least two inputs required");
  using state_t
    = detail::combine_latest_state<std::decay_t<F>,
                                   output_type_t<std::decay_t<Ts>>...>;
  using intermediate_type = typename state_t::intermediate_type;
  using mapped_type = std::optional<typename state_t::output_type>;
  auto state = std::make_shared<state_t>(std::in_place, std::forward<F>(fn));
  return parent()
    ->add_child_hdl(std::in_place_type<op::merge<intermediate_type>>,
                    sizeof...(Ts),
                    state_t::map(std::integral_constant<size_t, Indexes>{},
                                 std::forward<Ts>(inputs))
                      .as_observable()...)
    .map(
      [state](const intermediate_type& value) { return state->on_next(value); })
    .filter([](const mapped_type& mapped) { return mapped.has_value(); })
    .map([](const mapped_type& mapped) { return *mapped; });
}

} // namespace caf::flow

namespace caf::detail {

template <class...>
struct has_max_concurrent_arg : std::false_type {};

template <class T, class... Ts>
struct has_max_concurrent_arg<T, Ts...> {
  static constexpr bool value = std::is_same_v<std::decay_t<T>, size_t>;
};

template <class... Ts>
constexpr bool has_max_concurrent_arg_v = has_max_concurrent_arg<Ts...>::value;

} // namespace caf::detail

namespace caf::flow {

template <class T>
template <class Out, class... Inputs>
auto observable<T>::merge(Inputs&&... xs) {
  if constexpr (detail::has_max_concurrent_arg_v<Inputs...>) {
    return merge_with_concurrency(std::forward<Inputs>(xs)...);
  } else {
    if constexpr (sizeof...(Inputs) == 0) {
      return merge_with_concurrency(defaults::flow::max_concurrent,
                                    std::forward<Inputs>(xs)...);
    } else {
      return merge_with_concurrency(sizeof...(Inputs) + 1,
                                    std::forward<Inputs>(xs)...);
    }
  }
}

template <class T>
template <class F, class... Inputs>
auto observable<T>::combine_latest(F&& fn, Inputs&&... xs) {
  constexpr std::make_index_sequence<sizeof...(Inputs) + 1> indexes;
  return combine_latest_impl(std::forward<F>(fn), indexes, std::move(*this),
                             std::forward<Inputs>(xs)...);
}

template <class T>
template <class Out, class... Inputs>
auto observable<T>::concat(Inputs&&... xs) {
  if constexpr (is_observable_v<Out>) {
    static_assert(sizeof...(Inputs) == 0,
                  "concat on an observable<observable<T>> allows no arguments");
    using value_t = output_type_t<Out>;
    return parent()->add_child_hdl(std::in_place_type<op::concat<value_t>>,
                                   *this);
  } else {
    static_assert(sizeof...(Inputs) > 0, "no observable to concatenate");
    static_assert(
      (std::is_same_v<Out, output_type_t<std::decay_t<Inputs>>> && ...),
      "can only concatenate observables with the same observed type");
    return parent()->add_child_hdl(std::in_place_type<op::concat<Out>>, *this,
                                   std::forward<Inputs>(xs).as_observable()...);
  }
}

template <class T>
template <class Out, class F>
auto observable<T>::flat_map(F f, size_t max_concurrent) {
  using res_t = decltype(f(std::declval<const Out&>()));
  if constexpr (is_observable_v<res_t>) {
    return map([fn = std::move(f)](const Out& x) mutable {
             return fn(x).as_observable();
           })
      .merge(max_concurrent);
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
    return map([cptr = parent(), fn = std::move(f)](const Out& x) mutable {
             return cptr->make_observable().from_container(fn(x));
           })
      .concat();
  }
}

template <class T>
template <class Out, class F>
auto observable<T>::flat_map(F f) {
  return flat_map(std::move(f), defaults::flow::max_concurrent);
}

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
    return map([cptr = parent(), fn = std::move(f)](const Out& x) mutable {
             return cptr->make_observable().from_container(fn(x));
           })
      .concat();
  }
}

template <class T>
template <class F, class T0, class... Ts>
auto observable<T>::zip_with(F fn, T0 input0, Ts... inputs) {
  using output_type = op::zip_with_output_t<F, T,                     //
                                            typename T0::output_type, //
                                            typename Ts::output_type...>;
  if (pimpl_)
    return op::make_zip_with(pimpl_->parent(), std::move(fn), *this,
                             std::move(input0), std::move(inputs)...);
  return observable<output_type>{};
}

// -- observable: splitting ----------------------------------------------------

template <class T>
observable<cow_tuple<cow_vector<T>, observable<T>>>
observable<T>::prefix_and_tail(size_t n) {
  return parent()->add_child_hdl(std::in_place_type<op::prefix_and_tail<T>>,
                                 as_observable(), n);
}

template <class T>
observable<cow_tuple<T, observable<T>>> observable<T>::head_and_tail() {
  using prefix_tuple_t = cow_tuple<cow_vector<T>, observable<T>>;
  return prefix_and_tail(1)
    .map([](const prefix_tuple_t& tup) {
      auto& [vec, obs] = tup.data();
      CAF_ASSERT(vec.size() == 1);
      return make_cow_tuple(vec.front(), obs);
    })
    .as_observable();
}

// -- observable: multicasting -------------------------------------------------

template <class T>
connectable<T> observable<T>::publish() {
  return connectable<T>{make_counted<op::publish<T>>(parent(), pimpl_)};
}

template <class T>
observable<T> observable<T>::share(size_t subscriber_threshold) {
  return publish().ref_count(subscriber_threshold);
}

// -- observable: observing ----------------------------------------------------

template <class T>
observable<T> observable<T>::observe_on(coordinator* other, size_t buffer_size,
                                        size_t min_request_size) {
  auto [pull, push] = async::make_spsc_buffer_resource<T>(buffer_size,
                                                          min_request_size);
  subscribe(push);
  return other->add_child_hdl(std::in_place_type<op::from_resource<T>>,
                              std::move(pull));
}

// -- observable: converting ---------------------------------------------------

template <class T>
async::consumer_resource<T>
observable<T>::to_resource(size_t buffer_size, size_t min_request_size) {
  using buffer_type = async::spsc_buffer<T>;
  auto buf = make_counted<buffer_type>(buffer_size, min_request_size);
  auto up = make_counted<buffer_writer_impl<buffer_type>>(pimpl_->parent());
  up->init(buf);
  subscribe(up->as_observer());
  return async::consumer_resource<T>{std::move(buf)};
}

template <class T>
async::publisher<T> observable<T>::to_publisher() {
  return async::publisher<T>::from(*this);
}

template <class T>
template <class U>
stream observable<T>::to_stream(cow_string name, timespan max_delay,
                                size_t max_items_per_batch) {
  static_assert(std::is_same_v<T, U> && has_type_id_v<U>,
                "T must have a type ID when converting to a stream");
  using trait_t = detail::batching_trait<U>;
  using impl_t = flow::op::buffer<trait_t>;
  auto* pptr = parent();
  if (max_delay <= timespan::zero()) {
    auto what = make_error(sec::invalid_argument,
                           "stream operators require a positive delay");
    auto op = pptr->add_child(std::in_place_type<op::fail<async::batch>>,
                              std::move(what));
    return pptr->to_stream_impl(cow_string{std::move(name)}, std::move(op),
                                type_id_v<U>, max_items_per_batch);
  }
  auto obs = pptr->add_child_hdl(std::in_place_type<flow::op::interval>,
                                 max_delay, max_delay);
  auto batch_op = pptr->add_child(std::in_place_type<impl_t>,
                                  max_items_per_batch, *this, std::move(obs));
  return pptr->to_stream_impl(cow_string{std::move(name)}, std::move(batch_op),
                              type_id_v<U>, max_items_per_batch);
}

template <class T>
template <class U>
stream observable<T>::to_stream(std::string name, timespan max_delay,
                                size_t max_items_per_batch) {
  return to_stream<U>(cow_string{std::move(name)}, max_delay,
                      max_items_per_batch);
}

template <class T>
template <class U>
typed_stream<U> observable<T>::to_typed_stream(cow_string name,
                                               timespan max_delay,
                                               size_t max_items_per_batch) {
  auto res = to_stream<U>(std::move(name), max_delay, max_items_per_batch);
  return {res.source(), res.name(), res.id()};
}

template <class T>
template <class U>
typed_stream<U> observable<T>::to_typed_stream(std::string name,
                                               timespan max_delay,
                                               size_t max_items_per_batch) {
  auto res = to_stream<U>(std::move(name), max_delay, max_items_per_batch);
  return {res.source(), res.name(), res.id()};
}

} // namespace caf::flow
