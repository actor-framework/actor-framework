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

class subscription;

template <class T>
class single;

template <class T>
class observer;

template <class T>
class observable;

template <class T>
class connectable;

template <class In, class Out>
class processor;

template <class Generator, class... Steps>
class generation;

template <class Step, class... Steps>
class transformation;

template <class T>
struct is_observable {
  static constexpr bool value = false;
};

template <class T>
struct is_observable<observable<T>> {
  static constexpr bool value = true;
};

template <class Step, class... Steps>
struct is_observable<transformation<Step, Steps...>> {
  static constexpr bool value = true;
};

template <class Generator, class... Steps>
struct is_observable<generation<Generator, Steps...>> {
  static constexpr bool value = true;
};

template <class In, class Out>
struct is_observable<processor<In, Out>> {
  static constexpr bool value = true;
};

template <class T>
struct is_observable<single<T>> {
  static constexpr bool value = true;
};

template <class T>
constexpr bool is_observable_v = is_observable<T>::value;

template <class T>
struct is_observer {
  static constexpr bool value = false;
};

template <class T>
struct is_observer<observer<T>> {
  static constexpr bool value = true;
};

template <class Step, class... Steps>
struct is_observer<transformation<Step, Steps...>> {
  static constexpr bool value = true;
};

template <class In, class Out>
struct is_observer<processor<In, Out>> {
  static constexpr bool value = true;
};

template <class T>
constexpr bool is_observer_v = is_observer<T>::value;

class observable_builder;

template <class T>
struct input_type_oracle {
  using type = typename T::input_type;
};

template <class T>
using input_type_t = typename input_type_oracle<T>::type;

template <class T>
struct output_type_oracle {
  using type = typename T::output_type;
};

template <class T>
using output_type_t = typename output_type_oracle<T>::type;

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

template <class T>
using assert_scheduled_actor_hdr_t
  = std::enable_if_t<assert_scheduled_actor_hdr<T>::value, T>;

} // namespace caf::flow
