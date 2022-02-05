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
#include "caf/disposable.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/fwd.hpp"
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

  protected:
    disposable do_subscribe(observer_impl<T>* snk);
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
        ctx()->post_internally_fn([src = src_, snk = snk_, n] { //
          src->on_request(snk.get(), n);
        });
    }

    void cancel() override {
      if (src_) {
        ctx()->post_internally_fn([src = src_, snk = snk_] { //
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

private:
  intrusive_ptr<impl> pimpl_;
};

template <class T>
observable<T> observable<T>::impl::as_observable() noexcept {
  return observable<T>{intrusive_ptr{this}};
}

template <class T>
disposable observable<T>::impl::do_subscribe(observer_impl<T>* snk) {
  snk->on_subscribe(subscription{make_counted<sub_impl>(ctx(), this, snk)});
  // Note: we do NOT return the subscription here because this object is private
  //       to the observer. Outside code must call dispose() on the observer.
  return disposable{intrusive_ptr<typename disposable::impl>{snk}};
}

template <class T>
using observable_impl = typename observable<T>::impl;

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

/// Base type for processors with a buffer that broadcasts output to all
/// observers.
template <class T>
class buffered_observable_impl : public ref_counted, public observable_impl<T> {
public:
  // -- member types -----------------------------------------------------------

  using super = observable_impl<T>;

  using handle_type = observable<T>;

  struct output_t {
    size_t demand;
    observer<T> sink;
  };

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(buffered_observable_impl)

  // -- constructors, destructors, and assignment operators --------------------

  explicit buffered_observable_impl(coordinator* ctx)
    : ctx_(ctx), desired_capacity_(defaults::flow::buffer_size) {
    buf_.reserve(desired_capacity_);
  }

  buffered_observable_impl(coordinator* ctx, size_t desired_capacity)
    : ctx_(ctx), desired_capacity_(desired_capacity) {
    buf_.reserve(desired_capacity_);
  }

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override {
    CAF_LOG_TRACE("");
    if (!completed_) {
      completed_ = true;
      buf_.clear();
      for (auto& out : outputs_)
        out.sink.on_complete();
      outputs_.clear();
      do_on_complete();
    }
  }

  bool disposed() const noexcept override {
    return done() && outputs_.empty();
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

  void on_request(observer_impl<T>* sink, size_t n) override {
    CAF_LOG_TRACE(CAF_ARG(n));
    if (auto i = find(sink); i != outputs_.end()) {
      i->demand += n;
      update_max_demand();
      try_push();
    }
  }

  void on_cancel(observer_impl<T>* sink) override {
    CAF_LOG_TRACE("");
    if (auto i = find(sink); i != outputs_.end()) {
      outputs_.erase(i);
      if (outputs_.empty()) {
        shutdown();
      } else {
        update_max_demand();
        try_push();
      }
    }
  }

  disposable subscribe(observer<T> sink) override {
    if (done()) {
      sink.on_complete();
      return disposable{};
    } else {
      max_demand_ = 0;
      outputs_.emplace_back(output_t{0u, sink});
      return super::do_subscribe(sink.ptr());
    }
  }

  // -- properties -------------------------------------------------------------

  size_t has_observers() const noexcept {
    return !outputs_.empty();
  }

  virtual bool done() const noexcept {
    return completed_ && buf_.empty();
  }

  // -- buffer handling --------------------------------------------------------

  template <class Iterator, class Sentinel>
  void append_to_buf(Iterator first, Sentinel last) {
    buf_.insert(buf_.end(), first, last);
  }

  template <class Val>
  void append_to_buf(Val&& val) {
    buf_.emplace_back(std::forward<Val>(val));
  }

  /// Stops the source, but allows observers to still consume buffered data.
  virtual void shutdown() {
    CAF_LOG_TRACE("");
    if (!completed_) {
      completed_ = true;
      if (done()) {
        CAF_LOG_DEBUG("observable done, call on_complete on" << outputs_.size()
                                                             << "outputs");
        for (auto& out : outputs_)
          out.sink.on_complete();
        outputs_.clear();
        do_on_complete();
      } else {
        CAF_LOG_DEBUG("not done yet, delay on_complete calls");
      }
    }
  }

  /// Stops the source and drops any remaining data.
  virtual void abort(const error& reason) {
    CAF_LOG_TRACE(CAF_ARG(reason));
    if (!completed_) {
      completed_ = true;
      for (auto& out : outputs_)
        out.sink.on_error(reason);
      outputs_.clear();
      do_on_error(reason);
    }
  }

  /// Tries to push data from the buffer downstream.
  void try_push() {
    CAF_LOG_TRACE("");
    if (!batch_.empty()) {
      // Shortcuts nested calls to try_push. Can only be true if a sink calls
      // try_push in on_next.
      return;
    }
    size_t batch_size = std::min(desired_capacity_, defaults::flow::batch_size);
    while (max_demand_ > 0) {
      // Try to ship full batches.
      if (batch_size > buf_.size())
        pull(batch_size - buf_.size());
      auto n = std::min(max_demand_, buf_.size());
      if (n == 0)
        return;
      batch_.assign(std::make_move_iterator(buf_.begin()),
                    std::make_move_iterator(buf_.begin() + n));
      buf_.erase(buf_.begin(), buf_.begin() + n);
      auto items = span<const T>{batch_};
      for (auto& out : outputs_) {
        out.demand -= n;
        out.sink.on_next(items);
      }
      max_demand_ -= n;
      batch_.clear();
      if (done()) {
        for (auto& out : outputs_)
          out.sink.on_complete();
        outputs_.clear();
        do_on_complete();
        return;
      }
    }
  }

  auto find(observer_impl<T>* sink) {
    auto pred = [sink](auto& out) { return out.sink.ptr() == sink; };
    return std::find_if(outputs_.begin(), outputs_.end(), pred);
  }

protected:
  void update_max_demand() {
    if (outputs_.empty()) {
      max_demand_ = 0;
    } else {
      auto i = outputs_.begin();
      auto e = outputs_.end();
      auto init = (*i++).demand;
      auto f = [](size_t x, auto& out) { return std::min(x, out.demand); };
      max_demand_ = std::accumulate(i, e, init, f);
    }
  }

  coordinator* ctx_;
  size_t desired_capacity_;
  std::vector<T> buf_;
  bool completed_ = false;
  size_t max_demand_ = 0;
  std::vector<output_t> outputs_;

  /// Stores items right before calling on_next on the sinks.
  std::vector<T> batch_;

private:
  /// Customization point for generating more data.
  virtual void pull(size_t) {
    // nop
  }

  /// Customization point for adding cleanup logic.
  virtual void do_on_complete() {
    // nop
  }

  /// Customization point for adding error handling logic.
  virtual void do_on_error(const error&) {
    // nop
  }
};

template <class T>
using buffered_observable_impl_ptr = intrusive_ptr<buffered_observable_impl<T>>;

template <class T>
struct term_step {
  buffered_observable_impl<T>* pimpl;

  using output_type = T;

  bool on_next(const T& item) {
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

/// Base type for processors with a buffer that broadcasts output to all
/// observers.
template <class In, class Out>
class buffered_processor_impl : public buffered_observable_impl<Out>,
                                public processor_impl<In, Out> {
public:
  // -- member types -----------------------------------------------------------

  using super = buffered_observable_impl<Out>;

  using handle_type = processor<In, Out>;

  // -- constructors, destructors, and assignment operators --------------------

  explicit buffered_processor_impl(coordinator* ctx)
    : super(ctx, defaults::flow::buffer_size) {
    // nop
  }

  buffered_processor_impl(coordinator* ctx, size_t max_buffer_size)
    : super(ctx, max_buffer_size) {
    // nop
  }

  // -- disambiguation ---------------------------------------------------------

  observable<Out> as_observable() noexcept {
    return super::as_observable();
  }

  // -- implementation of disposable::impl -------------------------------------

  coordinator* ctx() const noexcept override {
    return super::ctx();
  }

  void dispose() override {
    if (!this->completed_) {
      if (sub_) {
        sub_.cancel();
        sub_ = nullptr;
      }
      super::dispose();
    }
  }

  bool disposed() const noexcept override {
    return super::disposed();
  }

  void ref_disposable() const noexcept override {
    this->ref();
  }

  void deref_disposable() const noexcept override {
    this->deref();
  }

  // -- implementation of observable<T>::impl ----------------------------------

  disposable subscribe(observer<Out> sink) override {
    return super::subscribe(std::move(sink));
  }

  void on_request(observer_impl<Out>* sink, size_t n) final {
    super::on_request(sink, n);
    try_fetch_more();
  }

  void on_cancel(observer_impl<Out>* sink) final {
    super::on_cancel(sink);
    try_fetch_more();
  }

  // -- implementation of observer<T>::impl ------------------------------------

  void on_subscribe(subscription sub) override {
    if (sub_) {
      sub.cancel();
    } else {
      sub_ = std::move(sub);
      in_flight_ = this->desired_capacity_;
      sub_.request(in_flight_);
    }
  }

  void on_next(span<const In> items) final {
    CAF_ASSERT(in_flight_ >= items.size());
    if (!this->completed_) {
      in_flight_ -= items.size();
      if (!do_on_next(items)) {
        this->try_push();
        shutdown();
      } else {
        this->try_push();
        try_fetch_more();
      }
    }
  }

  void on_complete() override {
    sub_ = nullptr;
    this->shutdown();
  }

  void on_error(const error& what) override {
    sub_ = nullptr;
    this->abort(what);
  }

  // -- overrides for buffered_observable_impl ---------------------------------

  void shutdown() override {
    super::shutdown();
    cancel_subscription();
  }

  void abort(const error& reason) override {
    super::abort(reason);
    cancel_subscription();
  }

protected:
  subscription sub_;
  size_t in_flight_ = 0;

private:
  void cancel_subscription() {
    if (sub_) {
      sub_.cancel();
      sub_ = nullptr;
    }
  }

  void try_fetch_more() {
    if (sub_) {
      auto abs = in_flight_ + this->buf_.size();
      if (this->desired_capacity_ > abs) {
        auto new_demand = this->desired_capacity_ - abs;
        in_flight_ += new_demand;
        sub_.request(new_demand);
      }
    }
  }

  /// Transforms input items to outputs.
  virtual bool do_on_next(span<const In> items) = 0;
};

/// Broadcasts its input to all observers without modifying it.
template <class T>
class broadcaster_impl : public buffered_processor_impl<T, T> {
public:
  using super = buffered_processor_impl<T, T>;

  using super::super;

private:
  bool do_on_next(span<const T> items) override {
    this->append_to_buf(items.begin(), items.end());
    return true;
  }
};

template <class T>
using broadcaster_impl_ptr = intrusive_ptr<broadcaster_impl<T>>;

// -- transformation -----------------------------------------------------------

template <class... Steps>
struct transform_processor_oracle;

template <class Step>
struct transform_processor_oracle<Step> {
  using type = typename Step::output_type;
};

template <class Step1, class Step2, class... Steps>
struct transform_processor_oracle<Step1, Step2, Steps...>
  : transform_processor_oracle<Step2, Steps...> {};

template <class... Steps>
using transform_processor_output_type_t =
  typename transform_processor_oracle<Steps...>::type;

/// A special type of observer that applies a series of transformation steps to
/// its input before broadcasting the result as output.
template <class Step, class... Steps>
class transformation final
  : public observable_def<transform_processor_output_type_t<Step, Steps...>> {
public:
  using input_type = typename Step::input_type;

  using output_type = transform_processor_output_type_t<Step, Steps...>;

  class impl : public buffered_processor_impl<input_type, output_type> {
  public:
    using super = buffered_processor_impl<input_type, output_type>;

    template <class... Ts>
    explicit impl(coordinator* ctx, Ts&&... steps)
      : super(ctx), steps(std::forward<Ts>(steps)...) {
      // nop
    }

    void on_complete() override {
      super::sub_ = nullptr;
      auto f = [this](auto& step, auto&... steps) {
        term_step<output_type> term{this};
        step.on_complete(steps..., term);
      };
      std::apply(f, steps);
    }

    void on_error(const error& what) override {
      super::sub_ = nullptr;
      auto f = [this, &what](auto& step, auto&... steps) {
        term_step<output_type> term{this};
        step.on_error(what, steps..., term);
      };
      std::apply(f, steps);
    }

    std::tuple<Step, Steps...> steps;

  private:
    bool do_on_next(span<const input_type> items) override {
      auto f = [this, items](auto& step, auto&... steps) {
        term_step<output_type> term{this};
        for (auto&& item : items)
          if (!step.on_next(item, steps..., term))
            return false;
        return true;
      };
      return std::apply(f, steps);
    }
  };

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
    auto pimpl = make_counted<impl>(source_.ptr()->ctx(), std::move(steps_));
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

/// Combines items from any number of observables.
template <class T>
class merger_impl : public buffered_observable_impl<T> {
public:
  using super = buffered_observable_impl<T>;

  using super::super;

  void concat_mode(bool new_value) {
    flags_.concat_mode = new_value;
  }

  class forwarder;

  friend class forwarder;

  class forwarder : public ref_counted, public observer_impl<T> {
  public:
    CAF_INTRUSIVE_PTR_FRIENDS(forwarder)

    explicit forwarder(intrusive_ptr<merger_impl> parent)
      : parent(std::move(parent)) {
      // nop
    }

    void on_complete() override {
      CAF_LOG_TRACE("");
      if (sub) {
        sub = nullptr;
        parent->forwarder_completed(this);
        parent = nullptr;
      }
    }

    void on_error(const error& what) override {
      CAF_LOG_TRACE(CAF_ARG(what));
      if (sub) {
        sub = nullptr;
        parent->forwarder_failed(this, what);
        parent = nullptr;
      }
    }

    void on_subscribe(subscription new_sub) override {
      CAF_LOG_TRACE("");
      if (!sub) {
        sub = std::move(new_sub);
        parent->forwarder_subscribed(this, sub);
      } else {
        new_sub.cancel();
      }
    }

    void on_next(span<const T> items) override {
      CAF_LOG_TRACE(CAF_ARG2("items.size", items.size()));
      if (parent)
        parent->on_batch(async::make_batch(items), this);
    }

    void dispose() override {
      CAF_LOG_TRACE("");
      on_complete();
    }

    bool disposed() const noexcept override {
      return !parent;
    }

    void ref_disposable() const noexcept final {
      this->ref();
    }

    void deref_disposable() const noexcept final {
      this->deref();
    }

    intrusive_ptr<merger_impl> parent;
    subscription sub;
  };

  explicit merger_impl(coordinator* ctx)
    : super(ctx, defaults::flow::batch_size) {
    // nop
  }

  disposable add(observable<T> source, intrusive_ptr<forwarder> fwd) {
    CAF_LOG_TRACE("");
    forwarders_.emplace_back(fwd);
    return source.subscribe(observer<T>{std::move(fwd)});
  }

  template <class Observable>
  disposable add(Observable source) {
    return add(std::move(source).as_observable(),
               make_counted<forwarder>(this));
  }

  bool done() const noexcept override {
    return super::done() && inputs_.empty() && forwarders_.empty();
  }

  void dispose() override {
    CAF_LOG_TRACE("");
    inputs_.clear();
    std::vector<fwd_ptr> fwds;
    fwds.swap(forwarders_);
    for (auto& fwd : fwds)
      fwd->dispose();
    super::dispose();
  }

  void cancel_inputs() {
    CAF_LOG_TRACE("");
    if (!this->completed_) {
      std::vector<fwd_ptr> fwds;
      fwds.swap(forwarders_);
      for (auto& fwd : fwds) {
        if (auto& sub = fwd->sub) {
          sub.cancel();
          sub = nullptr;
        }
      }
      this->shutdown();
    }
  }

  bool disposed() const noexcept override {
    return forwarders_.empty() && super::disposed();
  }

  void delay_error(bool value) {
    CAF_LOG_TRACE(CAF_ARG(value));
    flags_.delay_error = value;
  }

  void shutdown_on_last_complete(bool value) {
    CAF_LOG_TRACE(CAF_ARG(value));
    flags_.shutdown_on_last_complete = value;
    if (value && forwarders_.empty()) {
      if (delayed_error_)
        this->abort(delayed_error_);
      else
        this->shutdown();
    }
  }

  void on_error(const error& what) {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (!flags_.delay_error) {
      abort(what);
      return;
    }
    if (!delayed_error_)
      delayed_error_ = what;
  }

protected:
  void abort(const error& reason) override {
    CAF_LOG_TRACE(CAF_ARG(reason));
    super::abort(reason);
    inputs_.clear();
    forwarders_.clear();
  }

private:
  using fwd_ptr = intrusive_ptr<forwarder>;

  void pull(size_t n) override {
    CAF_LOG_TRACE(CAF_ARG(n));
    while (n > 0 && !inputs_.empty()) {
      auto& input = inputs_[0];
      auto m = std::min(input.buf.size() - input.offset, n);
      CAF_ASSERT(m > 0);
      auto items = input.buf.template items<T>().subspan(input.offset, m);
      this->append_to_buf(items.begin(), items.end());
      if (m + input.offset == input.buf.size()) {
        if (auto& sub = input.src->sub)
          sub.request(input.buf.size());
        inputs_.erase(inputs_.begin());
      } else {
        input.offset += m;
      }
      n -= m;
    }
  }

  void on_batch(async::batch buf, fwd_ptr src) {
    CAF_LOG_TRACE("");
    inputs_.emplace_back(buf, src);
    this->try_push();
  }

  void forwarder_subscribed(forwarder* ptr, subscription& sub) {
    CAF_LOG_TRACE("");
    if (!flags_.concat_mode || (!forwarders_.empty() && forwarders_[0] == ptr))
      sub.request(defaults::flow::buffer_size);
  }

  void forwarder_failed(forwarder* ptr, const error& what) {
    CAF_LOG_TRACE(CAF_ARG(what));
    if (!flags_.delay_error) {
      abort(what);
      return;
    }
    if (!delayed_error_)
      delayed_error_ = what;
    forwarder_completed(ptr);
  }

  void forwarder_completed(forwarder* ptr) {
    CAF_LOG_TRACE("");
    auto is_ptr = [ptr](auto& x) { return x == ptr; };
    auto i = std::find_if(forwarders_.begin(), forwarders_.end(), is_ptr);
    if (i != forwarders_.end()) {
      forwarders_.erase(i);
      CAF_LOG_DEBUG(forwarders_.size() << "forwarders remain");
      if (forwarders_.empty()) {
        if (flags_.shutdown_on_last_complete) {
          if (delayed_error_)
            this->abort(delayed_error_);
          else
            this->shutdown();
        }
      } else if (flags_.concat_mode) {
        if (auto& sub = forwarders_.front()->sub)
          sub.request(defaults::flow::buffer_size);
        // else: not subscribed yet, so forwarder_subscribed calls sub.request
      }
    }
  }

  struct input_t {
    size_t offset = 0;

    async::batch buf;

    fwd_ptr src;

    input_t(async::batch content, fwd_ptr source)
      : buf(std::move(content)), src(std::move(source)) {
      // nop
    }
  };

  struct flags_t {
    bool delay_error : 1;
    bool shutdown_on_last_complete : 1;
    bool concat_mode : 1;

    flags_t()
      : delay_error(false),
        shutdown_on_last_complete(true),
        concat_mode(false) {
      // nop
    }
  };

  std::vector<input_t> inputs_;
  std::vector<fwd_ptr> forwarders_;
  flags_t flags_;
  error delayed_error_;
};

template <class T>
using merger_impl_ptr = intrusive_ptr<merger_impl<T>>;

template <class T, class F>
class flat_map_observer_impl : public ref_counted, public observer_impl<T> {
public:
  using mapped_type = decltype((std::declval<F&>())(std::declval<const T&>()));

  using inner_type = typename mapped_type::output_type;

  CAF_INTRUSIVE_PTR_FRIENDS(flat_map_observer_impl)

  flat_map_observer_impl(coordinator* ctx, F f) : map_(std::move(f)) {
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
      merger_->on_error(what);
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

template <class T>
template <class F>
auto observable<T>::concat_map(F f) {
  using f_res = decltype(f(std::declval<const T&>()));
  static_assert(is_observable_v<f_res>,
                "mapping functions must return an observable");
  using impl_t = flat_map_observer_impl<T, F>;
  auto obs = make_counted<impl_t>(pimpl_->ctx(), std::move(f));
  obs->merger_ptr()->concat_mode(true);
  pimpl_->subscribe(obs->as_observer());
  return obs->merger();
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
        auto tptr = make_counted<broadcaster_impl<in_t>>(ctx_);
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

} // namespace caf::flow
