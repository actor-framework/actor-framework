// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/async/spsc_buffer.hpp"
#include "caf/defaults.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/flow/coordinator.hpp"
#include "caf/flow/gen/empty.hpp"
#include "caf/flow/gen/from_callable.hpp"
#include "caf/flow/gen/from_container.hpp"
#include "caf/flow/gen/iota.hpp"
#include "caf/flow/gen/just.hpp"
#include "caf/flow/gen/repeat.hpp"
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

// -- generation ---------------------------------------------------------------

/// Materializes an @ref observable from a `Generator` that produces items and
/// any number of processing steps that immediately transform the produced
/// items.
template <class Generator>
class generation_materializer {
public:
  using output_type = typename Generator::output_type;

  generation_materializer(coordinator* ctx, Generator generator)
    : ctx_(ctx), gen_(std::move(generator)) {
    // nop
  }

  generation_materializer() = delete;
  generation_materializer(const generation_materializer&) = delete;
  generation_materializer& operator=(const generation_materializer&) = delete;

  generation_materializer(generation_materializer&&) = default;
  generation_materializer& operator=(generation_materializer&&) = default;

  template <class... Steps>
  auto materialize(std::tuple<Steps...>&& steps) && {
    using impl_t = op::from_generator<Generator, Steps...>;
    return make_observable<impl_t>(ctx_, std::move(gen_), std::move(steps));
  }

  bool valid() const noexcept {
    return ctx_ != nullptr;
  }

private:
  coordinator* ctx_;
  Generator gen_;
};

// -- builder interface --------------------------------------------------------

/// Factory for @ref observable objects.
class CAF_CORE_EXPORT observable_builder {
public:
  friend class coordinator;

  observable_builder(const observable_builder&) noexcept = default;

  observable_builder& operator=(const observable_builder&) noexcept = default;

  /// Creates a @ref generation that emits values by repeatedly calling
  /// `generator.pull(...)`.
  template <class Generator>
  generation<Generator> from_generator(Generator generator) const {
    using materializer_t = generation_materializer<Generator>;
    return generation<Generator>{materializer_t{ctx_, std::move(generator)}};
  }

  /// Creates a @ref generation that emits `value` once.
  template <class T>
  generation<gen::just<T>> just(T value) const {
    return from_generator(gen::just<T>{std::move(value)});
  }

  /// Creates a @ref generation that emits `value` repeatedly.
  template <class T>
  generation<gen::repeat<T>> repeat(T value) const {
    return from_generator(gen::repeat<T>{std::move(value)});
  }

  /// Creates a @ref generation that emits ascending values.
  template <class T>
  generation<gen::iota<T>> iota(T value) const {
    return from_generator(gen::iota<T>{std::move(value)});
  }

  /// Creates an @ref observable without any values that calls `on_complete`
  /// after subscribing to it.
  template <class T>
  generation<gen::empty<T>> empty() {
    return from_generator(gen::empty<T>{});
  }

  /// Creates a @ref generation that emits ascending values.
  template <class Container>
  generation<gen::from_container<Container>>
  from_container(Container values) const {
    return from_generator(gen::from_container<Container>{std::move(values)});
  }

  /// Creates a @ref generation that emits ascending values.
  template <class F>
  generation<gen::from_callable<F>> from_callable(F fn) const {
    return from_generator(gen::from_callable<F>{std::move(fn)});
  }

  /// Creates a @ref generation that emits `num` ascending values, starting with
  /// `init`.
  template <class T>
  auto range(T init, size_t num) const {
    return iota(init).take(num);
  }

  /// Creates an @ref observable that reads and emits all values from `res`.
  template <class T>
  observable<T> from_resource(async::consumer_resource<T> res) const {
    using impl_t = op::from_resource<T>;
    return make_observable<impl_t>(ctx_, std::move(res));
  }

  /// Creates an @ref observable that emits a sequence of integers spaced by the
  /// @p period.
  /// @param initial_delay Delay of the first integer after subscribing.
  /// @param period Delay of each consecutive integer after the first value.
  template <class Rep, class Period>
  observable<int64_t> interval(std::chrono::duration<Rep, Period> initial_delay,
                               std::chrono::duration<Rep, Period> period) {
    return make_observable<op::interval>(ctx_, initial_delay, period);
  }

  /// Creates an @ref observable that emits a sequence of integers spaced by the
  /// @p delay.
  /// @param delay Time delay between two integer values.
  template <class Rep, class Period>
  observable<int64_t> interval(std::chrono::duration<Rep, Period> delay) {
    return interval(delay, delay);
  }

  /// Creates an @ref observable that emits a single item after the @p delay.
  template <class Rep, class Period>
  observable<int64_t> timer(std::chrono::duration<Rep, Period> delay) {
    return make_observable<op::interval>(ctx_, delay, delay, 1);
  }

  /// Creates an @ref observable without any values that also never terminates.
  template <class T>
  observable<T> never() {
    return make_observable<op::never<T>>(ctx_);
  }

  /// Creates an @ref observable without any values that fails immediately when
  /// subscribing to it by calling `on_error` on the subscriber.
  template <class T>
  observable<T> fail(error what) {
    return make_observable<op::fail<T>>(ctx_, std::move(what));
  }

  /// Create a fresh @ref observable for each @ref observer using the factory.
  template <class Factory>
  auto defer(Factory factory) {
    return make_observable<op::defer<Factory>>(ctx_, std::move(factory));
  }

  /// Creates an @ref observable that combines the items emitted from all passed
  /// source observables.
  template <class Input, class... Inputs>
  auto merge(Input x, Inputs... xs) {
    static_assert(is_observable_v<Input> && (is_observable_v<Inputs> && ...));
    using out_t = output_type_t<Input>;
    static_assert((std::is_same_v<out_t, output_type_t<Inputs>> && ...));
    using impl_t = op::merge<out_t>;
    return make_observable<impl_t>(ctx_, std::move(x).as_observable(),
                                   std::move(xs).as_observable()...);
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

  /// Creates an @ref observable that combines the emitted from all passed
  /// source observables by applying a function object.
  /// @param fn The zip function. Takes one element from each input at a time
  ///           and reduces them into a single result.
  /// @param input0 The input at index 0.
  /// @param input1 The input at index 1.
  /// @param inputs The inputs for index > 1, if any.
  template <class F, class T0, class T1, class... Ts>
  auto zip_with(F fn, T0 input0, T1 input1, Ts... inputs) {
    return op::make_zip_with(ctx_, std::move(fn), std::move(input0),
                             std::move(input1), std::move(inputs)...);
  }

private:
  explicit observable_builder(coordinator* ctx) : ctx_(ctx) {
    // nop
  }

  coordinator* ctx_;
};

} // namespace caf::flow
