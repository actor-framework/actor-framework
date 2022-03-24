// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"

#include <cstdint>

namespace caf::flow {

// -- forward declarations -----------------------------------------------------

template <class T>
class repeater_source;

template <class Container>
class container_source;

template <class T>
class value_source;

template <class F>
class callable_source;

// -- special-purpose observable implementations -------------------------------

class interval_action;

class CAF_CORE_EXPORT interval_impl : public ref_counted,
                                      public observable_impl<int64_t> {
public:
  // -- member types -----------------------------------------------------------

  using output_type = int64_t;

  using super = observable_impl<int64_t>;

  // -- friends ----------------------------------------------------------------

  CAF_INTRUSIVE_PTR_FRIENDS(interval_impl)

  friend class interval_action;

  // -- constructors, destructors, and assignment operators --------------------

  interval_impl(coordinator* ctx, timespan initial_delay, timespan period);

  interval_impl(coordinator* ctx, timespan initial_delay, timespan period,
                int64_t max_val);

  ~interval_impl() override;

  // -- implementation of disposable::impl -------------------------------------

  void dispose() override;

  bool disposed() const noexcept override;

  void ref_disposable() const noexcept override;

  void deref_disposable() const noexcept override;

  // -- implementation of observable<T>::impl ----------------------------------

  coordinator* ctx() const noexcept override;

  void on_request(observer_impl<int64_t>*, size_t) override;

  void on_cancel(observer_impl<int64_t>*) override;

  disposable subscribe(observer<int64_t> what) override;

private:
  void fire(interval_action*);

  coordinator* ctx_;
  observer<int64_t> obs_;
  disposable pending_;
  timespan initial_delay_;
  timespan period_;
  coordinator::steady_time_point last_;
  int64_t val_ = 0;
  int64_t max_;
  size_t demand_ = 0;
};

// -- builder interface --------------------------------------------------------

/// Factory for @ref observable objects.
class observable_builder {
public:
  friend class coordinator;

  observable_builder(const observable_builder&) noexcept = default;

  observable_builder& operator=(const observable_builder&) noexcept = default;

  /// Creates a @ref generation that emits `value` indefinitely.
  template <class T>
  [[nodiscard]] generation<repeater_source<T>> repeat(T value) const;

  /// Creates a @ref generation that emits all values from `values`.
  template <class Container>
  [[nodiscard]] generation<container_source<Container>>
  from_container(Container values) const;

  /// Creates a @ref generation that emits `value` once.
  template <class T>
  [[nodiscard]] generation<value_source<T>> just(T value) const;

  /// Creates a @ref generation that emits values by repeatedly calling `fn`.
  template <class F>
  [[nodiscard]] generation<callable_source<F>> from_callable(F fn) const;

  /// Creates a @ref generation that emits values by repeatedly calling
  /// `pullable.pull(...)`. For example implementations of the `Pullable`
  /// concept, see @ref container_source, @ref repeater_source and
  /// @ref callable_source.
  template <class Pullable>
  [[nodiscard]] generation<Pullable> lift(Pullable pullable) const;

  /// Creates an @ref observable that reads and emits all values from `res`.
  template <class T>
  [[nodiscard]] observable<T>
  from_resource(async::consumer_resource<T> res) const;

  /// Creates an @ref observable that emits a sequence of integers spaced by the
  /// @p period.
  /// @param initial_delay Delay of the first integer after subscribing.
  /// @param period Delay of each consecutive integer after the first value.
  template <class Rep, class Period>
  [[nodiscard]] observable<int64_t>
  interval(std::chrono::duration<Rep, Period> initial_delay,
           std::chrono::duration<Rep, Period> period) {
    // Intervals introduce a time-dependency, so we need to watch them in order
    // to prevent actors from shutting down while timeouts are still pending.
    auto ptr = make_counted<interval_impl>(ctx_, initial_delay, period);
    ctx_->watch(ptr->as_disposable());
    return observable<int64_t>{std::move(ptr)};
  }

  /// Creates an @ref observable that emits a sequence of integers spaced by the
  /// @p delay.
  /// @param delay Time delay between two integer values.
  template <class Rep, class Period>
  [[nodiscard]] observable<int64_t>
  interval(std::chrono::duration<Rep, Period> delay) {
    return interval(delay, delay);
  }

  /// Creates an @ref observable that emits a single item after the @p delay.
  template <class Rep, class Period>
  [[nodiscard]] observable<int64_t>
  timer(std::chrono::duration<Rep, Period> delay) {
    auto ptr = make_counted<interval_impl>(ctx_, delay, delay, 1);
    ctx_->watch(ptr->as_disposable());
    return observable<int64_t>{std::move(ptr)};
  }

  /// Creates an @ref observable without any values that simply calls
  /// `on_complete` after subscribing to it.
  template <class T>
  [[nodiscard]] observable<T> empty() {
    return observable<T>{make_counted<empty_observable_impl<T>>(ctx_)};
  }

private:
  explicit observable_builder(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  coordinator* ctx_;
};

// -- generation ---------------------------------------------------------------

/// Implements the `Pullable` concept for emitting values from a container.
template <class Container>
class container_source {
public:
  using output_type = typename Container::value_type;

  explicit container_source(Container&& values) : values_(std::move(values)) {
    pos_ = values_.begin();
  }

  container_source(container_source&&) = default;
  container_source& operator=(container_source&&) = default;

  container_source() = delete;
  container_source(const container_source&) = delete;
  container_source& operator=(const container_source&) = delete;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    while (pos_ != values_.end() && n > 0) {
      if (!step.on_next(*pos_++, steps...))
        return;
      --n;
    }
    if (pos_ == values_.end())
      step.on_complete(steps...);
  }

private:
  Container values_;
  typename Container::const_iterator pos_;
};

/// Implements the `Pullable` concept for emitting the same value repeatedly.
template <class T>
class repeater_source {
public:
  using output_type = T;

  explicit repeater_source(T value) : value_(std::move(value)) {
    // nop
  }

  repeater_source(repeater_source&&) = default;
  repeater_source(const repeater_source&) = default;
  repeater_source& operator=(repeater_source&&) = default;
  repeater_source& operator=(const repeater_source&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i)
      if (!step.on_next(value_, steps...))
        return;
  }

private:
  T value_;
};

/// Implements the `Pullable` concept for emitting the same value once.
template <class T>
class value_source {
public:
  using output_type = T;

  explicit value_source(T value) : value_(std::move(value)) {
    // nop
  }

  value_source(value_source&&) = default;
  value_source(const value_source&) = default;
  value_source& operator=(value_source&&) = default;
  value_source& operator=(const value_source&) = default;

  template <class Step, class... Steps>
  void pull(size_t, Step& step, Steps&... steps) {
    if (step.on_next(value_, steps...))
      step.on_complete(steps...);
  }

private:
  T value_;
};

/// Implements the `Pullable` concept for emitting values generated from a
/// function object.
template <class F>
class callable_source {
public:
  using output_type = std::decay_t<decltype(std::declval<F&>()())>;

  explicit callable_source(F fn) : fn_(std::move(fn)) {
    // nop
  }

  callable_source(callable_source&&) = default;
  callable_source& operator=(callable_source&&) = default;

  callable_source(const callable_source&) = delete;
  callable_source& operator=(const callable_source&) = delete;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i)
      if (!step.on_next(fn_(), steps...))
        return;
  }

private:
  F fn_;
};

/// Helper class for combining multiple generation and transformation steps into
/// a single @ref observable object.
template <class Generator, class... Steps>
class generation final
  : public observable_def<steps_output_type_t<Generator, Steps...>> {
public:
  using output_type = steps_output_type_t<Generator, Steps...>;

  class impl : public observable_impl_base<output_type> {
  public:
    using super = observable_impl_base<output_type>;

    template <class... Ts>
    impl(coordinator* ctx, Generator gen, Ts&&... steps)
      : super(ctx), gen_(std::move(gen)), steps_(std::forward<Ts>(steps)...) {
      // nop
    }

    // -- implementation of disposable::impl -----------------------------------

    void dispose() override {
      disposed_ = true;
      if (out_) {
        out_.on_complete();
        out_ = nullptr;
      }
    }

    bool disposed() const noexcept override {
      return disposed_;
    }

    // -- implementation of observable_impl<T> ---------------------------------

    disposable subscribe(observer<output_type> what) override {
      if (out_) {
        return super::reject_subscription(what, sec::too_many_observers);
      } else if (disposed_) {
        return super::reject_subscription(what, sec::disposed);
      } else {
        out_ = what;
        return super::do_subscribe(what);
      }
    }

    void on_request(observer_impl<output_type>* sink, size_t n) override {
      if (sink == out_.ptr()) {
        auto fn = [this, n](auto&... steps) {
          term_step<impl> term{this};
          gen_.pull(n, steps..., term);
        };
        std::apply(fn, steps_);
        push();
      }
    }

    void on_cancel(observer_impl<output_type>* sink) override {
      if (sink == out_.ptr()) {
        buf_.clear();
        out_ = nullptr;
        disposed_ = true;
      }
    }

    // -- callbacks for term_step ----------------------------------------------

    void append_to_buf(const output_type& item) {
      CAF_ASSERT(out_.valid());
      buf_.emplace_back(item);
    }

    void shutdown() {
      CAF_ASSERT(out_.valid());
      push();
      out_.on_complete();
      out_ = nullptr;
      disposed_ = true;
    }

    void abort(const error& reason) {
      CAF_ASSERT(out_.valid());
      push();
      out_.on_error(reason);
      out_ = nullptr;
      disposed_ = true;
    }

  private:
    void push() {
      if (!buf_.empty()) {
        out_.on_next(make_span(buf_));
        buf_.clear();
      }
    }

    Generator gen_;
    std::tuple<Steps...> steps_;
    observer<output_type> out_;
    bool disposed_ = false;
    std::vector<output_type> buf_;
  };

  template <class... Ts>
  generation(coordinator* ctx, Generator gen, Ts&&... steps)
    : ctx_(ctx), gen_(std::move(gen)), steps_(std::forward<Ts>(steps)...) {
    // nop
  }

  generation() = delete;
  generation(const generation&) = delete;
  generation& operator=(const generation&) = delete;

  generation(generation&&) = default;
  generation& operator=(generation&&) = default;

  /// @copydoc observable::transform
  template <class NewStep>
  generation<Generator, Steps..., NewStep> transform(NewStep step) && {
    static_assert(std::is_same_v<typename NewStep::input_type, output_type>,
                  "step object does not match the output type");
    return {ctx_, std::move(gen_),
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

  template <class Fn>
  auto map(Fn fn) && {
    return std::move(*this).transform(map_step<Fn>{std::move(fn)});
  }

  template <class F>
  auto flat_map_optional(F f) && {
    return std::move(*this).transform(flat_map_optional_step<F>{std::move(f)});
  }

  observable<output_type> as_observable() && override {
    auto pimpl = make_observable_impl<impl>(ctx_, std::move(gen_),
                                            std::move(steps_));
    return observable<output_type>{std::move(pimpl)};
  }

  coordinator* ctx() const noexcept {
    return ctx_;
  }

  constexpr bool valid() const noexcept {
    return true;
  }

private:
  coordinator* ctx_;
  Generator gen_;
  std::tuple<Steps...> steps_;
};

// -- observable_builder::repeat -----------------------------------------------

template <class T>
generation<repeater_source<T>> observable_builder::repeat(T value) const {
  return {ctx_, repeater_source<T>{std::move(value)}};
}

// -- observable_builder::from_container ---------------------------------------

template <class Container>
generation<container_source<Container>>
observable_builder::from_container(Container values) const {
  return {ctx_, container_source<Container>{std::move(values)}};
}

// -- observable_builder::just -------------------------------------------------

template <class T>
generation<value_source<T>> observable_builder::just(T value) const {
  return {ctx_, value_source<T>{std::move(value)}};
}

// -- observable_builder::from_callable ----------------------------------------

template <class F>
generation<callable_source<F>> observable_builder::from_callable(F fn) const {
  return {ctx_, callable_source<F>{std::move(fn)}};
}

// -- observable_builder::from_resource ----------------------------------------

template <class T>
observable<T>
observable_builder::from_resource(async::consumer_resource<T> hdl) const {
  using buffer_type = typename async::consumer_resource<T>::buffer_type;
  using res_t = observable<T>;
  if (auto buf = hdl.try_open()) {
    auto adapter = make_counted<observable_buffer_impl<buffer_type>>(ctx_, buf);
    buf->set_consumer(adapter);
    ctx_->watch(adapter->as_disposable());
    return res_t{std::move(adapter)};
  } else {
    auto err = make_error(sec::invalid_observable,
                          "from_resource: failed to open the resource");
    return res_t{make_counted<observable_error_impl<T>>(ctx_, std::move(err))};
  }
}

// -- observable_builder::lift -------------------------------------------------

template <class Pullable>
generation<Pullable> observable_builder::lift(Pullable pullable) const {
  return {ctx_, std::move(pullable)};
}

} // namespace caf::flow
