// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"

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
  : public observable_def<
      transform_processor_output_type_t<Generator, Steps...>> {
public:
  using output_type = transform_processor_output_type_t<Generator, Steps...>;

  class impl : public buffered_observable_impl<output_type> {
  public:
    using super = buffered_observable_impl<output_type>;

    template <class... Ts>
    impl(coordinator* ctx, Generator gen, Ts&&... steps)
      : super(ctx), gen_(std::move(gen)), steps_(std::forward<Ts>(steps)...) {
      // nop
    }

  private:
    virtual void pull(size_t n) {
      auto fn = [this, n](auto&... steps) {
        term_step<output_type> term{this};
        gen_.pull(n, steps..., term);
      };
      std::apply(fn, steps_);
    }

    Generator gen_;
    std::tuple<Steps...> steps_;
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
    auto pimpl = make_counted<impl>(ctx_, std::move(gen_), std::move(steps_));
    return observable<output_type>{std::move(pimpl)};
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
