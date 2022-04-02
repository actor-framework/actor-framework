// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstddef>
#include <numeric>
#include <type_traits>
#include <vector>

#include "caf/async/consumer.hpp"
#include "caf/async/producer.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/cow_tuple.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/unordered_flat_map.hpp"
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
#include "caf/flow/observable_state.hpp"
#include "caf/flow/observer.hpp"
#include "caf/flow/step.hpp"
#include "caf/flow/subscription.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/logger.hpp"
#include "caf/make_counted.hpp"
#include "caf/ref_counted.hpp"
#include "caf/sec.hpp"
#include "caf/span.hpp"

namespace caf::flow {

/// Represents a potentially unbound sequence of values.
template <class T>
class observable {
public:
  using output_type = T;

  /// Internal interface of an `observable`.
  class impl : public disposable::impl {
  public:
    // -- member types ---------------------------------------------------------

    using output_type = T;

    // -- properties -----------------------------------------------------------

    virtual coordinator* ctx() const noexcept = 0;

    // -- flow processing ------------------------------------------------------

    /// Subscribes a new observer.
    virtual disposable subscribe(observer<T> what) = 0;

    virtual void on_request(observer_impl<T>* sink, size_t n) = 0;

    virtual void on_cancel(observer_impl<T>* sink) = 0;

    observable as_observable() noexcept;

    /// Creates a subscription for `sink` and returns a @ref disposable to
    /// cancel the observer.
    disposable do_subscribe(observer_impl<T>* sink);

    /// @copydoc do_subscribe
    disposable do_subscribe(observer<T>& sink) {
      return do_subscribe(sink.ptr());
    }

    /// Calls `on_error` on the `sink` with given `code` and returns a
    /// default-constructed @ref disposable.
    disposable reject_subscription(observer_impl<T>* sink, sec code);

    /// @copydoc reject_subscription
    disposable reject_subscription(observer<T>& sink, sec code) {
      return reject_subscription(sink.ptr(), code);
    }
  };

  class sub_impl final : public ref_counted, public subscription::impl {
  public:
    using src_type = typename observable<T>::impl;

    using snk_type = typename observer<T>::impl;

    CAF_INTRUSIVE_PTR_FRIENDS(sub_impl)

    sub_impl(coordinator* ctx, src_type* src, snk_type* snk)
      : ctx_(ctx), src_(src), snk_(snk) {
      // nop
    }

    bool disposed() const noexcept override {
      return src_ == nullptr;
    }

    void ref_disposable() const noexcept override {
      this->ref();
    }

    void deref_disposable() const noexcept override {
      this->deref();
    }

    void request(size_t n) override {
      if (src_)
        ctx()->delay_fn([src = src_, snk = snk_, n] { //
          src->on_request(snk.get(), n);
        });
    }

    void cancel() override {
      if (src_) {
        ctx()->delay_fn([src = src_, snk = snk_] { //
          src->on_cancel(snk.get());
        });
        src_.reset();
        snk_.reset();
      }
    }

    auto* ctx() const noexcept {
      return ctx_;
    }

  private:
    coordinator* ctx_;
    intrusive_ptr<src_type> src_;
    intrusive_ptr<snk_type> snk_;
  };

  explicit observable(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
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

  void dispose() {
    if (pimpl_) {
      pimpl_->dispose();
      pimpl_ = nullptr;
    }
  }

  disposable as_disposable() const& noexcept {
    return disposable{pimpl_};
  }

  disposable as_disposable() && noexcept {
    return disposable{std::move(pimpl_)};
  }

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

  /// Returns a transformation that emits items by merging the outputs of all
  /// observables returned by `f`.
  template <class F>
  auto flat_map(F f);

  /// Returns a transformation that emits items from optional values returned by
  /// `f`.
  template <class F>
  transformation<flat_map_optional_step<F>> flat_map_optional(F f);

  /// Returns a transformation that emits items by concatenating the outputs of
  /// all observables returned by `f`.
  template <class F>
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

  bool valid() const noexcept {
    return pimpl_ != nullptr;
  }

  explicit operator bool() const noexcept {
    return valid();
  }

  bool operator!() const noexcept {
    return !valid();
  }

  impl* ptr() {
    return pimpl_.get();
  }

  const impl* ptr() const {
    return pimpl_.get();
  }

  const intrusive_ptr<impl>& as_intrusive_ptr() const& noexcept {
    return pimpl_;
  }

  intrusive_ptr<impl>&& as_intrusive_ptr() && noexcept {
    return std::move(pimpl_);
  }

  void swap(observable& other) {
    pimpl_.swap(other.pimpl_);
  }

  /// @pre `valid()`
  coordinator* ctx() const {
    return pimpl_->ctx();
  }

private:
  intrusive_ptr<impl> pimpl_;
};

template <class T>
observable<T> observable<T>::impl::as_observable() noexcept {
  return observable<T>{intrusive_ptr{this}};
}

template <class T>
disposable observable<T>::impl::do_subscribe(observer_impl<T>* sink) {
  sink->on_subscribe(subscription{make_counted<sub_impl>(ctx(), this, sink)});
  // Note: we do NOT return the subscription here because this object is private
  //       to the observer. Outside code must call dispose() on the observer.
  return disposable{intrusive_ptr<typename disposable::impl>{sink}};
}

template <class T>
disposable observable<T>::impl::reject_subscription(observer_impl<T>* sink, //
                                                    sec code) {
  sink->on_error(make_error(code));
  return disposable{};
}

/// @relates observable
template <class T>
using observable_impl = typename observable<T>::impl;

/// Default base type for observable implementation types.
template <class T>
class observable_impl_base : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = observable_impl<T>;

  using output_type = T;

  // -- constructors, destructors, and assignment operators --------------------

  explicit observable_impl_base(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  // -- implementation of observable_impl<T> ---------------------------------

  coordinator* ctx() const noexcept final {
    return ctx_;
  }

protected:
  // -- member variables -------------------------------------------------------

  coordinator* ctx_;
};

/// Convenience function for creating a sub-type of @ref observable_impl without
/// exposing the derived type.
template <class T, class... Ts>
intrusive_ptr<observable_impl<typename T::output_type>>
make_observable_impl(coordinator* ctx, Ts&&... xs) {
  using out_t = typename T::output_type;
  using res_t = intrusive_ptr<observable_impl<out_t>>;
  auto ptr = new T(ctx, std::forward<Ts>(xs)...);
  return res_t{static_cast<observable_impl<out_t>*>(ptr), false};
}

/// Base type for classes that represent a definition of an `observable` which
/// has not yet been converted to an actual `observable`.
template <class T>
class observable_def {
public:
  virtual ~observable_def() = default;

  template <class OnNext>
  auto for_each(OnNext on_next) && {
    return lift().for_each(std::move(on_next));
  }

  template <class OnNext, class OnError>
  auto for_each(OnNext on_next, OnError on_error) && {
    return lift().for_each(std::move(on_next), std::move(on_error));
  }

  template <class OnNext, class OnError, class OnComplete>
  auto for_each(OnNext on_next, OnError on_error, OnComplete on_complete) && {
    return lift().for_each(std::move(on_next), std::move(on_error),
                           std::move(on_complete));
  }

  template <class F>
  auto flat_map(F f) && {
    return lift().flat_map(std::move(f));
  }

  template <class F>
  auto concat_map(F f) && {
    return lift().concat_map(std::move(f));
  }

  observable<cow_tuple<std::vector<T>, observable<T>>>
  prefix_and_tail(size_t prefix_size) && {
    return lift().prefix_and_tail(prefix_size);
  }

  observable<cow_tuple<T, observable<T>>> head_and_tail() && {
    return lift().head_and_tail();
  }

  disposable subscribe(observer<T> what) && {
    return lift().subscribe(std::move(what));
  }

  disposable subscribe(async::producer_resource<T> resource) && {
    return lift().subscribe(std::move(resource));
  }

  async::consumer_resource<T> to_resource() && {
    return lift().to_resource();
  }

  async::consumer_resource<T> to_resource(size_t buffer_size,
                                          size_t min_request_size) && {
    return lift().to_resource(buffer_size, min_request_size);
  }

  observable<T> observe_on(coordinator* other) && {
    return lift().observe_on(other);
  }

  observable<T> observe_on(coordinator* other, size_t buffer_size,
                           size_t min_request_size) && {
    return lift().observe_on(other, buffer_size, min_request_size);
  }

  virtual observable<T> as_observable() && = 0;

private:
  decltype(auto) lift() {
    return std::move(*this).as_observable();
  }
};

template <class In, class Out>
class processor {
public:
  class impl : public observer_impl<In>, public observable_impl<Out> {
  public:
    observer_impl<In>* as_observer_ptr() noexcept {
      return this;
    }

    observable_impl<In>* as_observable_ptr() noexcept {
      return this;
    }
  };

  explicit processor(intrusive_ptr<impl> pimpl) noexcept
    : pimpl_(std::move(pimpl)) {
    // nop
  }

  processor& operator=(std::nullptr_t) noexcept {
    pimpl_.reset();
    return *this;
  }

  using input_type = In;

  using output_type = Out;

  processor() noexcept = default;
  processor(processor&&) noexcept = default;
  processor(const processor&) noexcept = default;
  processor& operator=(processor&&) noexcept = default;
  processor& operator=(const processor&) noexcept = default;

  // -- conversion -------------------------------------------------------------

  disposable as_disposable() const& noexcept {
    return as_observable().as_disposable();
  }

  disposable as_disposable() && noexcept {
    return std::move(*this).as_observable().as_disposable();
  }

  observer<In> as_observer() const& noexcept {
    return pimpl_->as_observer();
  }

  observer<In> as_observer() && noexcept {
    auto raw = pimpl_.release()->as_observer_ptr();
    auto ptr = intrusive_ptr<observer_impl<In>>{raw, false};
    return observer<In>{std::move(ptr)};
  }

  observable<Out> as_observable() const& noexcept {
    return pimpl_->as_observable();
  }

  observable<Out> as_observable() && noexcept {
    auto raw = pimpl_.release()->as_observable_ptr();
    auto ptr = intrusive_ptr<observable_impl<Out>>{raw, false};
    return observable<Out>{std::move(ptr)};
  }

private:
  intrusive_ptr<impl> pimpl_;
};

template <class In, class Out = In>
using processor_impl = typename processor<In, Out>::impl;

// -- representing an error as an observable -----------------------------------

template <class T>
class observable_error_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(observable_error_impl)

  // -- constructors, destructors, and assignment operators --------------------

  observable_error_impl(coordinator* ctx, error what)
    : ctx_(ctx), what_(std::move(what)) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    // nop
  }

  bool disposed() const noexcept override {
    return true;
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<T>*, size_t) override {
    CAF_RAISE_ERROR("observable_error_impl::on_request called");
  }

  void on_cancel(observer_impl<T>*) override {
    CAF_RAISE_ERROR("observable_error_impl::on_cancel called");
  }

  disposable subscribe(observer<T> what) override {
    what.on_error(what_);
    return {};
  }

private:
  coordinator* ctx_;
  error what_;
};

// -- broadcasting -------------------------------------------------------------

template <class Impl>
struct term_step {
  Impl* pimpl;

  using output_type = typename Impl::output_type;

  bool on_next(const output_type& item) {
    pimpl->append_to_buf(item);
    return true;
  }

  void on_complete() {
    pimpl->shutdown();
  }

  void on_error(const error& what) {
    pimpl->abort(what);
  }
};

/// Broadcasts its input to all observers without modifying it.
template <class Step, class... Steps>
class broadcaster_impl
  : public ref_counted,
    public processor_impl<typename Step::input_type,
                          steps_output_type_t<Step, Steps...>> {
public:
  // -- member types -----------------------------------------------------------

  using input_type = typename Step::input_type;

  using output_type = steps_output_type_t<Step, Steps...>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(broadcaster_impl)

  // -- constructors, destructors, and assignment operators --------------------

  template <class... Ts>
  explicit broadcaster_impl(coordinator* ctx, Ts&&... step_args)
    : ctx_(ctx), steps_(std::forward<Ts>(step_args)...) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    CAF_LOG_TRACE("");
    term_.dispose();
  }

  bool disposed() const noexcept override {
    return !term_.active();
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observer<T>::impl ------------------------------------

  void on_subscribe(subscription sub) override {
    if (term_.start(sub))
      sub_ = std::move(sub);
  }

  void on_next(span<const input_type> items) override {
    if (!term_.finalized()) {
      auto f = [this, items](auto& step, auto&... steps) {
        for (auto&& item : items)
          if (!step.on_next(item, steps..., term_))
            return false;
        return true;
      };
      auto still_running = std::apply(f, steps_);
      term_.push();
      if (!still_running && sub_) {
        CAF_ASSERT(!term_.active());
        sub_.cancel();
        sub_ = nullptr;
      }
    }
  }

  void on_complete() override {
    if (term_.active()) {
      auto f = [this](auto& step, auto&... steps) {
        step.on_complete(steps..., term_);
      };
      std::apply(f, steps_);
      sub_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (term_.active()) {
      auto f = [this, &what](auto& step, auto&... steps) {
        step.on_error(what, steps..., term_);
      };
      std::apply(f, steps_);
      sub_ = nullptr;
    }
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<output_type>* sink, size_t n) override {
    CAF_LOG_TRACE(CAF_ARG(n));
    term_.on_request(sub_, sink, n);
  }

  void on_cancel(observer_impl<output_type>* sink) override {
    CAF_LOG_TRACE("");
    term_.on_cancel(sub_, sink);
  }

  disposable subscribe(observer<output_type> sink) override {
    return term_.add(this, sink);
  }

  // -- properties -------------------------------------------------------------

  size_t buffered() const noexcept {
    return term_.buffered();
  }

  observable_state state() const noexcept {
    return term_.state();
  }

  const error& err() const noexcept {
    return term_.err();
  }

protected:
  /// Points to the coordinator that runs this observable.
  coordinator* ctx_;

  /// Allows us to request more items.
  subscription sub_;

  /// The processing steps that we apply before pushing data downstream.
  std::tuple<Step, Steps...> steps_;

  /// Pushes data to the observers.
  broadcast_step<output_type> term_;
};

/// @relates broadcaster_impl
template <class Step, class... Steps>
using broadcaster_impl_ptr = intrusive_ptr<broadcaster_impl<Step, Steps...>>;

/// @relates broadcaster_impl
template <class T>
broadcaster_impl_ptr<identity_step<T>> make_broadcaster_impl(coordinator* ctx) {
  return make_counted<broadcaster_impl<identity_step<T>>>(ctx);
}

/// @relates broadcaster_impl
template <class Step, class... Steps>
broadcaster_impl_ptr<Step, Steps...>
make_broadcaster_impl_from_tuple(coordinator* ctx,
                                 std::tuple<Step, Steps...>&& tup) {
  auto f = [ctx](Step&& step, Steps&&... steps) {
    return make_counted<broadcaster_impl<Step, Steps...>>(ctx, std::move(step),
                                                          std::move(steps)...);
  };
  return std::apply(f, std::move(tup));
}

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

  auto take(size_t n) && {
    return std::move(*this).transform(limit_step<output_type>{n});
  }

  template <class Predicate>
  auto filter(Predicate predicate) && {
    return std::move(*this).transform(
      filter_step<Predicate>{std::move(predicate)});
  }

  template <class F>
  auto map(F f) && {
    return std::move(*this).transform(map_step<F>{std::move(f)});
  }

  template <class F>
  auto flat_map_optional(F f) && {
    return std::move(*this).transform(flat_map_optional_step<F>{std::move(f)});
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
    auto pimpl = make_broadcaster_impl_from_tuple(source_.ptr()->ctx(),
                                                  std::move(steps_));
    auto res = pimpl->as_observable();
    source_.subscribe(observer<input_type>{std::move(pimpl)});
    source_ = nullptr;
    return res;
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
  subscribe(observer<T>{obs});
  return std::move(obs).as_disposable();
}

template <class T>
template <class OnNext, class OnError>
disposable observable<T>::for_each(OnNext on_next, OnError on_error) {
  auto obs = make_observer(std::move(on_next), std::move(on_error));
  subscribe(obs);
  return std::move(obs).as_disposable();
}

template <class T>
template <class OnNext, class OnError, class OnComplete>
disposable observable<T>::for_each(OnNext on_next, OnError on_error,
                                   OnComplete on_complete) {
  auto obs = make_observer(std::move(on_next), std::move(on_error),
                           std::move(on_complete));
  subscribe(obs);
  return std::move(obs).as_disposable();
}

// -- observable::flat_map -----------------------------------------------------

/// @relates merger_impl
template <class T>
struct merger_input {
  explicit merger_input(observable<T> in) : in(std::move(in)) {
    // nop
  }

  /// Stores a handle to the input observable for delayed subscription.
  observable<T> in;

  /// The subscription to this input.
  subscription sub;

  /// Stores received items until the merger can forward them downstream.
  std::vector<T> buf;
};

/// Combines items from any number of observables.
template <class T>
class merger_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = observable_impl<T>;

  using input_t = merger_input<T>;

  using input_ptr = std::unique_ptr<input_t>;

  using input_key = size_t;

  using input_map = detail::unordered_flat_map<input_key, input_ptr>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(merger_impl)

  template <class, class, class>
  friend class flow::forwarder;

  // -- constructors, destructors, and assignment operators --------------------

  explicit merger_impl(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    for (auto& kvp : inputs_) {
      auto& input = *kvp.second;
      if (input.in) {
        input.in = nullptr;
      }
      if (input.sub) {
        input.sub.cancel();
        input.sub = nullptr;
      }
    }
    inputs_.clear();
    term_.dispose();
  }

  bool disposed() const noexcept override {
    return term_.finalized();
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<T>* sink, size_t demand) override {
    if (auto n = term_.on_request(sink, demand); n > 0) {
      pull(n);
      term_.push();
    }
  }

  void on_cancel(observer_impl<T>* sink) override {
    if (auto n = term_.on_cancel(sink); n > 0) {
      pull(n);
      term_.push();
    }
  }

  disposable subscribe(observer<T> sink) override {
    // On the first subscribe, we subscribe to our inputs unless the user did
    // not add any inputs before that. In that case, we close immediately except
    // when running with shutdown_on_last_complete turned off.
    if (term_.idle() && inputs_.empty() && flags_.shutdown_on_last_complete)
      term_.close();
    auto res = term_.add(this, sink);
    if (res && term_.start()) {
      for (auto& [key, input] : inputs_) {
        using fwd_impl = forwarder<T, merger_impl, size_t>;
        auto fwd = make_counted<fwd_impl>(this, key);
        input->in.subscribe(fwd->as_observer());
      }
    }
    return res;
  }

  // -- dynamic input management -----------------------------------------------

  template <class Observable>
  void add(Observable source) {
    switch (term_.state()) {
      case observable_state::idle:
        // Only add to the inputs but don't subscribe yet.
        emplace(std::move(source).as_observable());
        break;
      case observable_state::running: {
        // Add and subscribe.
        auto& [key, input] = emplace(std::move(source).as_observable());
        using fwd_impl = forwarder<T, merger_impl, size_t>;
        auto fwd = make_counted<fwd_impl>(this, key);
        input->in.subscribe(fwd->as_observer());
        break;
      }
      default:
        // In any other case, this turns into a no-op.
        break;
    }
  }

  // -- properties -------------------------------------------------------------

  size_t buffered() {
    return std::accumulate(inputs_.begin(), inputs_.end(), size_t{0},
                           [](size_t tmp, const merger_input<T>& in) {
                             return tmp + in.buf.size();
                           });
  }

  bool shutdown_on_last_complete() const noexcept {
    return flags_.shutdown_on_last_complete;
  }

  void shutdown_on_last_complete(bool new_value) noexcept {
    flags_.shutdown_on_last_complete = new_value;
    if (new_value && inputs_.empty())
      term_.fin();
  }

private:
  typename input_map::value_type& emplace(observable<T> source) {
    auto& vec = inputs_.container();
    vec.emplace_back(next_key_++, std::make_unique<input_t>(std::move(source)));
    return vec.back();
  }

  void fwd_on_subscribe(input_key key, subscription sub) {
    if (!term_.finalized()) {
      if (auto i = inputs_.find(key); i != inputs_.end()) {
        auto& in = *i->second;
        sub.request(max_pending_);
        in.sub = std::move(sub);
      } else {
        sub.cancel();
      }
    } else {
      sub.cancel();
    }
  }

  void drop_if_empty(typename input_map::iterator i) {
    auto& in = *i->second;
    if (in.buf.empty()) {
      inputs_.erase(i);
      if (inputs_.empty() && flags_.shutdown_on_last_complete)
        term_.fin();
    } else {
      in.in = nullptr;
      in.sub = nullptr;
    }
  }

  void fwd_on_complete(input_key key) {
    if (auto i = inputs_.find(key); i != inputs_.end())
      drop_if_empty(i);
  }

  void fwd_on_error(input_key key, const error& what) {
    if (auto i = inputs_.find(key); i != inputs_.end()) {
      if (!term_.err()) {
        term_.err(what);
        if (flags_.delay_error) {
          drop_if_empty(i);
        } else {
          auto& in = *i->second;
          if (!in.buf.empty())
            term_.on_next(in.buf);
          term_.fin();
          for (auto j = inputs_.begin(); j != inputs_.end(); ++j)
            if (j != i && j->second->sub)
              j->second->sub.cancel();
          inputs_.clear();
        }
      } else {
        drop_if_empty(i);
      }
    }
  }

  void fwd_on_next(input_key key, span<const T> items) {
    if (auto i = inputs_.find(key); i != inputs_.end()) {
      auto& in = *i->second;
      if (!term_.finalized()) {
        if (auto n = std::min(term_.min_demand(), items.size()); n > 0) {
          term_.on_next(items.subspan(0, n));
          term_.push();
          if (in.sub)
            in.sub.request(n);
          items = items.subspan(n);
        }
        if (!items.empty())
          in.buf.insert(in.buf.end(), items.begin(), items.end());
      }
    }
  }

  void pull(size_t n) {
    // Must not be re-entered. Any on_request call must use the event loop.
    CAF_ASSERT(!pulling_);
    if (inputs_.empty()) {
      if (flags_.shutdown_on_last_complete)
        term_.fin();
      return;
    }
    CAF_DEBUG_STMT(pulling_ = true);
    auto& in_vec = inputs_.container();
    for (size_t i = 0; n > 0 && i < inputs_.size(); ++i) {
      auto index = (pos_ + 1) % inputs_.size();
      auto& in = *in_vec[index].second;
      if (auto m = std::min(in.buf.size(), n); m > 0) {
        n -= m;
        auto items = make_span(in.buf.data(), m);
        term_.on_next(items);
        in.buf.erase(in.buf.begin(), in.buf.begin() + m);
        if (in.sub) {
          in.sub.request(m);
          pos_ = index;
        } else if (!in.in && in.buf.empty()) {
          in_vec.erase(in_vec.begin() + index);
          if (in_vec.empty()) {
            if (flags_.shutdown_on_last_complete)
              term_.fin();
            CAF_DEBUG_STMT(pulling_ = false);
            return;
          }
        } else {
          pos_ = index;
        }
      }
    }
    CAF_DEBUG_STMT(pulling_ = false);
  }

  struct flags_t {
    bool delay_error : 1;
    bool shutdown_on_last_complete : 1;

    flags_t() : delay_error(false), shutdown_on_last_complete(true) {
      // nop
    }
  };

  /// Points to our scheduling context.
  coordinator* ctx_;

  /// Fine-tunes the behavior of the merger.
  flags_t flags_;

  /// Configures how many items we buffer per input.
  size_t max_pending_ = defaults::flow::buffer_size;

  /// Stores the last round-robin read position.
  size_t pos_ = 0;

  /// Associates inputs with ascending keys.
  input_map inputs_;

  /// Stores the last round-robin read position.
  size_t next_key_ = 0;

  /// Pushes items to subscribed observers.
  broadcast_step<T> term_;

#ifdef CAF_ENABLE_RUNTIME_CHECKS
  /// Protect against re-entering `pull`.
  bool pulling_ = false;
#endif
};

template <class T>
using merger_impl_ptr = intrusive_ptr<merger_impl<T>>;

template <class T, class F>
class flat_map_observer_impl : public ref_counted, public observer_impl<T> {
public:
  using mapped_type = decltype((std::declval<F&>())(std::declval<const T&>()));

  using inner_type = typename mapped_type::output_type;

  CAF_INTRUSIVE_PTR_FRIENDS(flat_map_observer_impl)

  flat_map_observer_impl(coordinator* ctx, F f)
    : ctx_(ctx), map_(std::move(f)) {
    merger_.emplace(ctx);
    merger_->shutdown_on_last_complete(false);
  }

  void dispose() override {
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
      merger_->shutdown_on_last_complete(true);
      merger_ = nullptr;
    }
  }

  bool disposed() const noexcept override {
    return merger_ != nullptr;
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  void on_complete() override {
    if (sub_) {
      sub_ = nullptr;
      merger_->shutdown_on_last_complete(true);
      merger_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (sub_) {
      sub_ = nullptr;
      merger_->shutdown_on_last_complete(true);
      auto obs = make_counted<observable_error_impl<inner_type>>(ctx_, what);
      merger_->add(obs->as_observable());
      merger_ = nullptr;
    }
  }

  void on_subscribe(subscription sub) override {
    if (!sub_ && merger_) {
      sub_ = std::move(sub);
      sub_.request(10);
    } else {
      sub.cancel();
    }
  }

  void on_next(span<const T> observables) override {
    if (sub_) {
      for (const auto& x : observables)
        merger_->add(map_(x).as_observable());
      sub_.request(observables.size());
    }
  }

  observable<inner_type> merger() {
    return observable<inner_type>{merger_};
  }

  auto& merger_ptr() {
    return merger_;
  }

private:
  coordinator* ctx_;
  subscription sub_;
  F map_;
  intrusive_ptr<merger_impl<inner_type>> merger_;
};

template <class T>
template <class F>
auto observable<T>::flat_map(F f) {
  using f_res = decltype(f(std::declval<const T&>()));
  static_assert(is_observable_v<f_res>,
                "mapping functions must return an observable");
  using impl_t = flat_map_observer_impl<T, F>;
  auto obs = make_counted<impl_t>(pimpl_->ctx(), std::move(f));
  pimpl_->subscribe(obs->as_observer());
  return obs->merger();
}

// -- observable::flat_map_optional --------------------------------------------

template <class T>
template <class F>
transformation<flat_map_optional_step<F>>
observable<T>::flat_map_optional(F f) {
  using step_type = flat_map_optional_step<F>;
  static_assert(std::is_same_v<typename step_type::input_type, T>,
                "flat_map_optional function does not match the input type");
  return {*this, std::forward_as_tuple(step_type{std::move(f)})};
}

// -- observable::concat_map ---------------------------------------------------

/// Combines items from any number of observables.
template <class T>
class concat_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = observable_impl<T>;

  using input_key = size_t;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(concat_impl)

  template <class, class, class>
  friend class flow::forwarder;

  // -- constructors, destructors, and assignment operators --------------------

  explicit concat_impl(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    if (sub_)
      sub_.cancel();
    inputs_.clear();
    term_.dispose();
  }

  bool disposed() const noexcept override {
    return term_.finalized();
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<T>* sink, size_t demand) override {
    if (auto n = term_.on_request(sink, demand); n > 0) {
      in_flight_ += n;
      if (sub_)
        sub_.request(n);
    }
  }

  void on_cancel(observer_impl<T>* sink) override {
    if (auto n = term_.on_cancel(sink); n > 0) {
      in_flight_ += n;
      if (sub_)
        sub_.request(n);
    }
  }

  disposable subscribe(observer<T> sink) override {
    // On the first subscribe, we subscribe to our inputs unless the user did
    // not add any inputs before that. In that case, we close immediately except
    // when running with shutdown_on_last_complete turned off.
    if (term_.idle() && inputs_.empty() && flags_.shutdown_on_last_complete)
      term_.close();
    auto res = term_.add(this, sink);
    if (res && term_.start() && !inputs_.empty())
      subscribe_next();
    return res;
  }

  // -- dynamic input management -----------------------------------------------

  template <class Observable>
  void add(Observable source) {
    switch (term_.state()) {
      case observable_state::idle:
        inputs_.emplace_back(std::move(source).as_observable());
        break;
      case observable_state::running:
        inputs_.emplace_back(std::move(source).as_observable());
        if (inputs_.size() == 1)
          subscribe_next();
        break;
      default:
        // In any other case, this turns into a no-op.
        break;
    }
  }

  // -- properties -------------------------------------------------------------

  bool shutdown_on_last_complete() const noexcept {
    return flags_.shutdown_on_last_complete;
  }

  void shutdown_on_last_complete(bool new_value) noexcept {
    flags_.shutdown_on_last_complete = new_value;
    if (new_value && inputs_.empty())
      term_.fin();
  }

private:
  void subscribe_next() {
    CAF_ASSERT(!inputs_.empty());
    CAF_ASSERT(!sub_);
    auto input = inputs_.front();
    ++in_key_;
    using fwd_impl = forwarder<T, concat_impl, size_t>;
    auto fwd = make_counted<fwd_impl>(this, in_key_);
    input.subscribe(fwd->as_observer());
  }

  void fwd_on_subscribe(input_key key, subscription sub) {
    if (in_key_ == key && term_.active()) {
      sub_ = std::move(sub);
      if (in_flight_ > 0)
        sub_.request(in_flight_);
    } else {
      sub.cancel();
    }
  }

  void fwd_on_complete(input_key key) {
    if (in_key_ == key) {
      inputs_.erase(inputs_.begin());
      sub_ = nullptr;
      if (!inputs_.empty()) {
        subscribe_next();
      } else if (flags_.shutdown_on_last_complete) {
        term_.fin();
      }
    }
  }

  void fwd_on_error(input_key key, const error& what) {
    if (in_key_ == key) {
      if (!flags_.delay_error) {
        term_.on_error(what);
        sub_ = nullptr;
        inputs_.clear();
      } else if (!term_.err()) {
        term_.err(what);
        fwd_on_complete(key);
      } else {
        fwd_on_complete(key);
      }
    }
  }

  void fwd_on_next(input_key key, span<const T> items) {
    if (in_key_ == key && !term_.finalized()) {
      CAF_ASSERT(in_flight_ >= items.size());
      in_flight_ -= items.size();
      term_.on_next(items);
      term_.push();
    }
  }

  struct flags_t {
    bool delay_error : 1;
    bool shutdown_on_last_complete : 1;

    flags_t() : delay_error(false), shutdown_on_last_complete(true) {
      // nop
    }
  };

  /// Points to our scheduling context.
  coordinator* ctx_;

  /// Fine-tunes the behavior of the concat.
  flags_t flags_;

  /// Stores our input sources. The first input is active (subscribed to) while
  /// the others are pending (not subscribed to).
  std::vector<observable<T>> inputs_;

  /// Our currently active subscription.
  subscription sub_;

  /// Identifies the forwarder.
  input_key in_key_ = 0;

  /// Stores how much demand we have left. When switching to a new input, we
  /// pass any demand unused by the previous input to the new one.
  size_t in_flight_ = 0;

  /// Pushes items to subscribed observers.
  broadcast_step<T> term_;
};

template <class T, class F>
class concat_map_observer_impl : public ref_counted, public observer_impl<T> {
public:
  using mapped_type = decltype((std::declval<F&>())(std::declval<const T&>()));

  using inner_type = typename mapped_type::output_type;

  CAF_INTRUSIVE_PTR_FRIENDS(concat_map_observer_impl)

  concat_map_observer_impl(coordinator* ctx, F f)
    : ctx_(ctx), map_(std::move(f)) {
    concat_.emplace(ctx);
    concat_->shutdown_on_last_complete(false);
  }

  void dispose() override {
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
      concat_->shutdown_on_last_complete(true);
      concat_ = nullptr;
    }
  }

  bool disposed() const noexcept override {
    return concat_ != nullptr;
  }

  void ref_disposable() const noexcept final {
    this->ref();
  }

  void deref_disposable() const noexcept final {
    this->deref();
  }

  void on_complete() override {
    if (sub_) {
      sub_ = nullptr;
      concat_->shutdown_on_last_complete(true);
      concat_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    if (sub_) {
      sub_ = nullptr;
      concat_->shutdown_on_last_complete(true);
      auto obs = make_counted<observable_error_impl<inner_type>>(ctx_, what);
      concat_->add(obs->as_observable());
      concat_ = nullptr;
    }
  }

  void on_subscribe(subscription sub) override {
    if (!sub_ && concat_) {
      sub_ = std::move(sub);
      sub_.request(10);
    } else {
      sub.cancel();
    }
  }

  void on_next(span<const T> observables) override {
    if (sub_) {
      for (const auto& x : observables)
        concat_->add(map_(x).as_observable());
      sub_.request(observables.size());
    }
  }

  observable<inner_type> concat() {
    return observable<inner_type>{concat_};
  }

  auto& concat_ptr() {
    return concat_;
  }

private:
  coordinator* ctx_;
  subscription sub_;
  F map_;
  intrusive_ptr<concat_impl<inner_type>> concat_;
};

template <class T>
template <class F>
auto observable<T>::concat_map(F f) {
  using f_res = decltype(f(std::declval<const T&>()));
  static_assert(is_observable_v<f_res>,
                "mapping functions must return an observable");
  using impl_t = concat_map_observer_impl<T, F>;
  auto obs = make_counted<impl_t>(pimpl_->ctx(), std::move(f));
  pimpl_->subscribe(obs->as_observer());
  return obs->concat();
}

// -- observable::prefix_and_tail ----------------------------------------------

template <class T>
class prefix_and_tail_observable_impl final
  : public ref_counted,
    public observable_impl<T>, // For the forwarding to the 'tail'.
    public processor_impl<T, cow_tuple<std::vector<T>, observable<T>>> {
public:
  // -- member types -----------------------------------------------------------

  using in_t = T;

  using out_t = cow_tuple<std::vector<T>, observable<T>>;

  using in_obs_t = observable_impl<in_t>;

  using out_obs_t = observable_impl<out_t>;

  // -- constructors, destructors, and assignment operators --------------------

  prefix_and_tail_observable_impl(coordinator* ctx, size_t prefix_size)
    : ctx_(ctx), prefix_size_(prefix_size) {
    // nop
  }

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(prefix_and_tail_observable_impl)

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
    }
    if (obs_) {
      obs_.on_complete();
      obs_ = nullptr;
    }
    if (tail_) {
      tail_.on_complete();
      tail_ = nullptr;
    }
  }

  bool disposed() const noexcept override {
    return !sub_ && !obs_ && !tail_;
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<in_t>::impl -------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  disposable subscribe(observer<in_t> sink) override {
    if (sink.ptr() == tail_.ptr()) {
      return in_obs_t::do_subscribe(sink.ptr());
    } else {
      sink.on_error(make_error(sec::invalid_observable));
      return disposable{};
    }
  }

  void on_request(observer_impl<in_t>*, size_t n) override {
    if (sub_)
      sub_.request(n);
  }

  void on_cancel(observer_impl<in_t>*) override {
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
    }
  }

  // -- implementation of observable<out_t>::impl ------------------------------

  disposable subscribe(observer<out_t> sink) override {
    obs_ = sink;
    return out_obs_t::do_subscribe(sink.ptr());
  }

  void on_request(observer_impl<out_t>*, size_t) override {
    if (sub_ && !requested_prefix_) {
      requested_prefix_ = true;
      sub_.request(prefix_size_);
    }
  }

  void on_cancel(observer_impl<out_t>*) override {
    // Only has an effect when canceling immediately. Otherwise, we forward to
    // tail_ and the original observer no longer is of any interest since it
    // receives at most one item anyways.
    if (sub_ && !tail_) {
      sub_.cancel();
      sub_ = nullptr;
    }
  }

  // -- implementation of observer<in_t>::impl ---------------------------------

  void on_subscribe(subscription sub) override {
    if (!had_subscriber_) {
      had_subscriber_ = true;
      sub_ = std::move(sub);
    } else {
      sub.cancel();
    }
  }

  void on_next(span<const in_t> items) override {
    if (tail_) {
      tail_.on_next(items);
    } else if (obs_) {
      CAF_ASSERT(prefix_.size() + items.size() <= prefix_size_);
      prefix_.insert(prefix_.end(), items.begin(), items.end());
      if (prefix_.size() >= prefix_size_) {
        auto tptr = make_broadcaster_impl<in_t>(ctx_);
        tail_ = tptr->as_observer();
        static_cast<observable_impl<in_t>*>(this)->subscribe(tail_);
        auto item = make_cow_tuple(std::move(prefix_), tptr->as_observable());
        obs_.on_next(make_span(&item, 1));
        obs_.on_complete();
        obs_ = nullptr;
      }
    }
  }

  void on_complete() override {
    sub_ = nullptr;
    if (obs_) {
      obs_.on_complete();
      obs_ = nullptr;
    }
    if (tail_) {
      tail_.on_complete();
      tail_ = nullptr;
    }
  }

  void on_error(const error& what) override {
    sub_ = nullptr;
    if (obs_) {
      obs_.on_error(what);
      obs_ = nullptr;
    }
    if (tail_) {
      tail_.on_error(what);
      tail_ = nullptr;
    }
  }

private:
  /// Our context.
  coordinator* ctx_;

  /// Subscription to the input data.
  subscription sub_;

  /// Handle for the observer that gets the prefix + tail tuple.
  observer<out_t> obs_;

  /// Handle to the tail for forwarding any data after the prefix.
  observer<in_t> tail_;

  /// Makes sure we only respect the first subscriber.
  bool had_subscriber_ = false;

  /// Stores whether we have requested the prefix;
  bool requested_prefix_ = false;

  /// Buffer for storing the prefix elements.
  std::vector<in_t> prefix_;

  /// User-defined size of the prefix.
  size_t prefix_size_;
};

template <class T>
observable<cow_tuple<std::vector<T>, observable<T>>>
observable<T>::prefix_and_tail(size_t prefix_size) {
  using impl_t = prefix_and_tail_observable_impl<T>;
  using out_t = cow_tuple<std::vector<T>, observable<T>>;
  auto obs = make_counted<impl_t>(pimpl_->ctx(), prefix_size);
  pimpl_->subscribe(obs->as_observer());
  return static_cast<observable_impl<out_t>*>(obs.get())->as_observable();
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

// -- observable::to_resource --------------------------------------------------

/// Reads from an observable buffer and emits the consumed items.
/// @note Only supports a single observer.
template <class Buffer>
class observable_buffer_impl
  : public ref_counted,
    public observable_impl<typename Buffer::value_type>,
    public async::consumer {
public:
  // -- member types -----------------------------------------------------------

  using value_type = typename Buffer::value_type;

  using buffer_ptr = intrusive_ptr<Buffer>;

  using super = observable_impl<value_type>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(observable_buffer_impl)

  // -- constructors, destructors, and assignment operators --------------------

  observable_buffer_impl(coordinator* ctx, buffer_ptr buf)
    : ctx_(ctx), buf_(buf) {
    // Unlike regular observables, we need a strong reference to the context.
    // Otherwise, the buffer might call schedule_fn on a destroyed object.
    this->ctx()->ref_coordinator();
  }

  ~observable_buffer_impl() {
    if (buf_)
      buf_->cancel();
    this->ctx()->deref_coordinator();
  }

  // -- implementation of disposable::impl -------------------------------------

  coordinator* ctx() const noexcept final {
    return ctx_;
  }

  void dispose() override {
    CAF_LOG_TRACE("");
    if (buf_) {
      buf_->cancel();
      buf_ = nullptr;
      if (dst_) {
        dst_.on_complete();
        dst_ = nullptr;
      }
    }
  }

  bool disposed() const noexcept override {
    return buf_ == nullptr;
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  void on_request(observer_impl<value_type>*, size_t n) override {
    CAF_LOG_TRACE(CAF_ARG(n));
    demand_ += n;
    if (demand_ == n)
      pull();
  }

  void on_cancel(observer_impl<value_type>*) override {
    CAF_LOG_TRACE("");
    dst_ = nullptr;
    dispose();
  }

  disposable subscribe(observer<value_type> what) override {
    CAF_LOG_TRACE("");
    if (buf_ && !dst_) {
      CAF_LOG_DEBUG("add destination");
      dst_ = std::move(what);
      return super::do_subscribe(dst_.ptr());
    } else {
      CAF_LOG_DEBUG("already have a destination");
      auto err = make_error(sec::cannot_add_upstream,
                            "observable buffers support only one observer");
      what.on_error(err);
      return disposable{};
    }
  }

  // -- implementation of async::consumer: these may get called concurrently ---

  void on_producer_ready() override {
    // nop
  }

  void on_producer_wakeup() override {
    CAF_LOG_TRACE("");
    this->ctx()->schedule_fn([ptr{strong_ptr()}] {
      CAF_LOG_TRACE("");
      ptr->pull();
    });
  }

  void ref_consumer() const noexcept override {
    this->ref();
  }

  void deref_consumer() const noexcept override {
    this->deref();
  }

protected:
  coordinator* ctx_;

private:
  void pull() {
    CAF_LOG_TRACE("");
    if (!buf_ || pulling_ || !dst_)
      return;
    pulling_ = true;
    struct decorator {
      size_t* demand;
      typename observer<value_type>::impl* dst;
      void on_next(span<const value_type> items) {
        CAF_LOG_TRACE(CAF_ARG(items));
        CAF_ASSERT(!items.empty());
        CAF_ASSERT(*demand >= items.empty());
        *demand -= items.size();
        CAF_LOG_DEBUG("got" << items.size() << "items");
        dst->on_next(items);
      }
      void on_complete() {
        CAF_LOG_TRACE("");
        dst->on_complete();
      }
      void on_error(const error& what) {
        CAF_LOG_TRACE(CAF_ARG(what));
        dst->on_error(what);
      }
    };
    decorator dst{&demand_, dst_.ptr()};
    if (!buf_->pull(async::prioritize_errors, demand_, dst).first) {
      buf_ = nullptr;
      dst_ = nullptr;
    }
    pulling_ = false;
  }

  intrusive_ptr<observable_buffer_impl> strong_ptr() {
    return {this};
  }

  buffer_ptr buf_;

  /// Stores a pointer to the target observer running on `remote_ctx_`.
  observer<value_type> dst_;

  bool pulling_ = false;

  size_t demand_ = 0;
};

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
  using buffer_type = async::spsc_buffer<T>;
  auto buf = make_counted<buffer_type>(buffer_size, min_request_size);
  subscribe(async::producer_resource<T>{buf});
  auto adapter = make_counted<observable_buffer_impl<buffer_type>>(other, buf);
  buf->set_consumer(adapter);
  other->watch(adapter->as_disposable());
  return observable<T>{std::move(adapter)};
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
    pimpl_->ctx()->watch(obs.as_disposable());
    return subscribe(std::move(obs));
  } else {
    CAF_LOG_DEBUG("failed to open producer resource");
    return {};
  }
}

// -- custom operators ---------------------------------------------------------

/// An observable that represents an empty range. As soon as an observer
/// requests values from this observable, it calls `on_complete`.
template <class T>
class empty_observable_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(empty_observable_impl)

  // -- constructors, destructors, and assignment operators --------------------

  explicit empty_observable_impl(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    // nop
  }

  bool disposed() const noexcept override {
    return true;
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<output_type>* snk, size_t) override {
    snk->on_complete();
  }

  void on_cancel(observer_impl<output_type>*) override {
    // nop
  }

  disposable subscribe(observer<output_type> sink) override {
    return this->do_subscribe(sink.ptr());
  }

private:
  coordinator* ctx_;
};

/// An observable that never calls any callbacks on its subscribers.
template <class T>
class mute_observable_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(mute_observable_impl)

  // -- constructors, destructors, and assignment operators --------------------

  explicit mute_observable_impl(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    if (!disposed_) {
      disposed_ = true;
      for (auto& obs : observers_)
        obs.on_complete();
    }
  }

  bool disposed() const noexcept override {
    return disposed_;
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<output_type>*, size_t) override {
    // nop
  }

  void on_cancel(observer_impl<output_type>*) override {
    // nop
  }

  disposable subscribe(observer<output_type> sink) override {
    if (!disposed_) {
      auto ptr = make_counted<subscription::nop_impl>();
      sink.ptr()->on_subscribe(subscription{std::move(ptr)});
      observers_.emplace_back(sink);
      return sink.as_disposable();
    } else {
      return this->reject_subscription(sink, sec::disposed);
    }
  }

private:
  coordinator* ctx_;
  bool disposed_ = false;
  std::vector<observer<output_type>> observers_;
};

/// An observable with minimal internal logic. Useful for writing unit tests.
template <class T>
class passive_observable : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = T;

  using super = observable_impl<T>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(passive_observable)

  // -- constructors, destructors, and assignment operators --------------------

  passive_observable(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    if (is_active(state)) {
      demand = 0;
      if (out) {
        out.on_complete();
        out = nullptr;
      }
      state = observable_state::completed;
    }
  }

  bool disposed() const noexcept override {
    return !is_active(state);
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable_impl<T> -----------------------------------

  coordinator* ctx() const noexcept override {
    return ctx_;
  }

  void on_request(observer_impl<output_type>* sink, size_t n) override {
    if (out.ptr() == sink) {
      demand += n;
    }
  }

  void on_cancel(observer_impl<output_type>* sink) override {
    if (out.ptr() == sink) {
      demand = 0;
      out = nullptr;
      state = observable_state::completed;
    }
  }

  disposable subscribe(observer<output_type> sink) override {
    if (state == observable_state::idle) {
      state = observable_state::running;
      out = sink;
      return this->do_subscribe(sink.ptr());
    } else if (is_final(state)) {
      return this->reject_subscription(sink, sec::disposed);
    } else {
      return this->reject_subscription(sink, sec::too_many_observers);
    }
  }

  // -- pushing items ----------------------------------------------------------

  void push(span<const output_type> items) {
    if (items.size() > demand)
      CAF_RAISE_ERROR("observables must not emit more items than requested");
    demand -= items.size();
    out.on_next(items);
  }

  void push(const output_type& item) {
    push(make_span(&item, 1));
  }

  template <class... Ts>
  void push(output_type x0, output_type x1, Ts... xs) {
    output_type items[] = {std::move(x0), std::move(x1), std::move(xs)...};
    push(make_span(items, sizeof...(Ts) + 2));
  }

  void complete() {
    if (is_active(state)) {
      demand = 0;
      if (out) {
        out.on_complete();
        out = nullptr;
      }
      state = observable_state::completed;
    }
  }

  void abort(const error& reason) {
    if (is_active(state)) {
      demand = 0;
      if (out) {
        out.on_error(reason);
        out = nullptr;
      }
      state = observable_state::aborted;
    }
  }

  // -- member variables -------------------------------------------------------

  observable_state state = observable_state::idle;

  size_t demand = 0;

  observer<T> out;

private:
  coordinator* ctx_;
};

/// @relates passive_observable
template <class T>
intrusive_ptr<passive_observable<T>> make_passive_observable(coordinator* ctx) {
  return make_counted<passive_observable<T>>(ctx);
}

} // namespace caf::flow
