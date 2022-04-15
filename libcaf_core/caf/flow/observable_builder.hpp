// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/detail/ref_counted_base.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/observable.hpp"
#include "caf/flow/op/defer.hpp"
#include "caf/flow/op/empty.hpp"
#include "caf/flow/op/fail.hpp"
#include "caf/flow/op/from_generator.hpp"
#include "caf/flow/op/from_resource.hpp"
#include "caf/flow/op/interval.hpp"
#include "caf/flow/op/never.hpp"
#include "caf/flow/op/zip_with.hpp"

#include <cstdint>
#include <functional>

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

// -- builder interface --------------------------------------------------------

/// Factory for @ref observable objects.
class CAF_CORE_EXPORT observable_builder {
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

  /// Creates a @ref generation that emits ascending values.
  template <class T>
  [[nodiscard]] auto iota(T init) const {
    return from_callable([x = std::move(init)]() mutable { return x++; });
  }

  /// Creates a @ref generation that emits `num` ascending values, starting with
  /// `init`.
  template <class T>
  [[nodiscard]] auto range(T init, size_t num) const {
    return iota(init).take(num);
  }

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
    return make_observable<op::interval>(ctx_, initial_delay, period);
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
    return make_observable<op::interval>(ctx_, delay, delay, 1);
  }

  /// Creates an @ref observable without any values that calls `on_complete`
  /// after subscribing to it.
  template <class T>
  [[nodiscard]] observable<T> empty() {
    return make_observable<op::empty<T>>(ctx_);
  }

  /// Creates an @ref observable without any values that also never terminates.
  template <class T>
  [[nodiscard]] observable<T> never() {
    return make_observable<op::never<T>>(ctx_);
  }

  /// Creates an @ref observable without any values that fails immediately when
  /// subscribing to it by calling `on_error` on the subscriber.
  template <class T>
  [[nodiscard]] observable<T> fail(error what) {
    return make_observable<op::fail<T>>(ctx_, std::move(what));
  }

  /// Create a fresh @ref observable for each @ref observer using the factory.
  template <class Factory>
  [[nodiscard]] auto defer(Factory factory) {
    return make_observable<op::defer<Factory>>(ctx_, std::move(factory));
  }

  /// Creates an @ref observable that combines the items emitted from all passed
  /// source observables.
  template <class Input, class... Inputs>
  auto merge(Input&& x, Inputs&&... xs) {
    using in_t = std::decay_t<Input>;
    if constexpr (is_observable_v<in_t>) {
      using impl_t = op::merge<output_type_t<in_t>>;
      return make_observable<impl_t>(ctx_, std::forward<Input>(x),
                                     std::forward<Inputs>(xs)...);
    } else {
      static_assert(detail::is_iterable_v<in_t>);
      using val_t = typename in_t::value_type;
      static_assert(is_observable_v<val_t>);
      using impl_t = op::merge<output_type_t<val_t>>;
      return make_observable<impl_t>(ctx_, std::forward<Input>(x),
                                     std::forward<Inputs>(xs)...);
    }
  }

  /// Creates an @ref observable that concatenates the items emitted from all
  /// passed source observables.
  template <class Input, class... Inputs>
  auto concat(Input&& x, Inputs&&... xs) {
    using in_t = std::decay_t<Input>;
    if constexpr (is_observable_v<in_t>) {
      using impl_t = op::concat<output_type_t<in_t>>;
      return make_observable<impl_t>(ctx_, std::forward<Input>(x),
                                     std::forward<Inputs>(xs)...);
    } else {
      static_assert(detail::is_iterable_v<in_t>);
      using val_t = typename in_t::value_type;
      static_assert(is_observable_v<val_t>);
      using impl_t = op::concat<output_type_t<val_t>>;
      return make_observable<impl_t>(ctx_, std::forward<Input>(x),
                                     std::forward<Inputs>(xs)...);
    }
  }

  /// @param fn The zip function. Takes one element from each input at a time
  ///           and reduces them into a single result.
  /// @param input0 The input at index 0.
  /// @param input1 The input at index 1.
  /// @param inputs The inputs for index > 1, if any.
  template <class F, class T0, class T1, class... Ts>
  auto zip_with(F fn, T0 input0, T1 input1, Ts... inputs) {
    using output_type = op::zip_with_output_t<F, //
                                              typename T0::output_type,
                                              typename T1::output_type,
                                              typename Ts::output_type...>;
    using impl_t = op::zip_with<F,                        //
                                typename T0::output_type, //
                                typename T1::output_type, //
                                typename Ts::output_type...>;
    if (input0.valid() && input1.valid() && (inputs.valid() && ...)) {
      auto ptr = make_counted<impl_t>(ctx_, std::move(fn),
                                      std::move(input0).as_observable(),
                                      std::move(input1).as_observable(),
                                      std::move(inputs).as_observable()...);
      return observable<output_type>{std::move(ptr)};
    } else {
      auto ptr = make_counted<op::empty<output_type>>(ctx_);
      return observable<output_type>{std::move(ptr)};
    }
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

  container_source() = default;
  container_source(container_source&&) = default;
  container_source& operator=(container_source&&) = default;

  container_source(const container_source& other) : values_(other.values_) {
    pos_ = values_.begin();
    std::advance(pos_, std::distance(other.values_.begin(), other.pos_));
  }
  container_source& operator=(const container_source& other) {
    values_ = other.values_;
    pos_ = values_.begin();
    std::advance(pos_, std::distance(other.values_.begin(), other.pos_));
    return *this;
  }

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    CAF_LOG_TRACE(CAF_ARG(n));
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
    CAF_LOG_TRACE(CAF_ARG(n));
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
  void pull([[maybe_unused]] size_t n, Step& step, Steps&... steps) {
    CAF_LOG_TRACE(CAF_ARG(n));
    CAF_ASSERT(n > 0);
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
  using callable_res_t = std::decay_t<decltype(std::declval<F&>()())>;

  static constexpr bool boxed_output = detail::is_optional_v<callable_res_t>;

  using output_type = detail::unboxed_t<callable_res_t>;

  explicit callable_source(F fn) : fn_(std::move(fn)) {
    // nop
  }

  callable_source(callable_source&&) = default;
  callable_source(const callable_source&) = default;
  callable_source& operator=(callable_source&&) = default;
  callable_source& operator=(const callable_source&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    CAF_LOG_TRACE(CAF_ARG(n));
    for (size_t i = 0; i < n; ++i) {
      if constexpr (boxed_output) {
        auto val = fn_();
        if (!val) {
          step.on_complete(steps...);
          return;
        } else if (!step.on_next(*val, steps...))
          return;
      } else {
        if (!step.on_next(fn_(), steps...))
          return;
      }
    }
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

  template <class Fn>
  auto map(Fn fn) && {
    return std::move(*this).transform(map_step<Fn>{std::move(fn)});
  }

  template <class... Inputs>
  auto merge(Inputs&&... xs) && {
    return std::move(*this).as_observable().merge(std::forward<Inputs>(xs)...);
  }

  template <class... Inputs>
  auto concat(Inputs&&... xs) && {
    return std::move(*this).as_observable().concat(std::forward<Inputs>(xs)...);
  }

  template <class F>
  auto flat_map_optional(F f) && {
    return std::move(*this).transform(flat_map_optional_step<F>{std::move(f)});
  }

  template <class F>
  auto do_on_next(F f) {
    return std::move(*this) //
      .transform(do_on_next_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_on_complete(F f) && {
    return std::move(*this).transform(
      do_on_complete_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_on_error(F f) && {
    return std::move(*this).transform(
      do_on_error_step<output_type, F>{std::move(f)});
  }

  template <class F>
  auto do_finally(F f) && {
    return std::move(*this).transform(
      do_finally_step<output_type, F>{std::move(f)});
  }

  observable<output_type> as_observable() && override {
    using impl_t = op::from_generator<Generator, Steps...>;
    return make_observable<impl_t>(ctx_, std::move(gen_), std::move(steps_));
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
  using impl_t = op::from_resource<T>;
  return make_observable<impl_t>(ctx_, std::move(hdl));
}

// -- observable_builder::lift -------------------------------------------------

template <class Pullable>
generation<Pullable> observable_builder::lift(Pullable pullable) const {
  return {ctx_, std::move(pullable)};
}

} // namespace caf::flow
