// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/fwd.hpp"

#include <type_traits>

namespace caf::detail {

/// Always evaluates to `Left`.
template <class Left, class Right>
struct left_oracle {
  using type = Left;
};

/// Useful to force dependent evaluation of Left. For example in template
/// functions where only Right is actually a template parameter.
template <class Left, class Right>
using left_t = typename left_oracle<Left, Right>::type;

} // namespace caf::detail

namespace caf::flow {

class coordinator;

using coordinator_ptr = intrusive_ptr<coordinator>;

class coordinated;

using coordinated_ptr = intrusive_ptr<coordinated>;

class subscription;

template <class T>
class single;

template <class T>
class observer;

template <class T>
class observable;

template <class Materializer, class... Steps>
class observable_def;

template <class Generator>
class generation_materializer;

template <class T>
class multicaster;

/// A blueprint for an @ref observer that generates items and applies any number
/// of processing steps immediately before emitting them.
template <class Generator, class... Steps>
using generation = observable_def<generation_materializer<Generator>, Steps...>;

template <class Input>
class transformation_materializer;

/// A blueprint for an @ref observer that applies a series of transformation
/// steps to its inputs and emits the results.
template <class Step, class... Steps>
using transformation
  = observable_def<transformation_materializer<typename Step::input_type>, Step,
                   Steps...>;

template <class T>
class connectable;

template <class T>
struct is_observable {
  static constexpr bool value = false;
};

template <class T>
struct is_observable<observable<T>> {
  static constexpr bool value = true;
};

template <class Materializer, class... Steps>
struct is_observable<observable_def<Materializer, Steps...>> {
  static constexpr bool value = true;
};

template <class T>
struct is_observable<single<T>> {
  static constexpr bool value = true;
};

template <class T>
constexpr bool is_observable_v = is_observable<T>::value;

class observable_builder;

template <class T>
struct input_type_oracle {
  using type = typename T::input_type;
};

template <class T>
using input_type_t = typename input_type_oracle<T>::type;

template <class...>
struct output_type_oracle;

template <class T>
struct output_type_oracle<T> {
  using type = typename T::output_type;
};

template <class T0, class T1, class... Ts>
struct output_type_oracle<T0, T1, Ts...> : output_type_oracle<T1, Ts...> {};

template <class... Ts>
using output_type_t = typename output_type_oracle<Ts...>::type;

template <class>
struct has_impl_include {
  static constexpr bool value = false;
};

template <class T>
constexpr bool has_impl_include_v = has_impl_include<T>::value;

template <class T>
struct assert_scheduled_actor_hdr {
  static constexpr bool value
    = has_impl_include_v<detail::left_t<scheduled_actor, T>>;
  static_assert(value,
                "include 'caf/scheduled_actor/flow.hpp' for this method");
};

template <class T, class V = T>
using assert_scheduled_actor_hdr_t
  = std::enable_if_t<assert_scheduled_actor_hdr<T>::value, V>;

} // namespace caf::flow

namespace caf::flow::op {

template <class T>
class base;

} // namespace caf::flow::op
