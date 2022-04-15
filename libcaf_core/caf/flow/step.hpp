// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_list.hpp"
#include "caf/detail/type_traits.hpp"
#include "caf/disposable.hpp"
#include "caf/error.hpp"
#include "caf/flow/observable_state.hpp"
#include "caf/flow/observer.hpp"

#include <algorithm>
#include <numeric>
#include <type_traits>
#include <unordered_set>

namespace caf::flow {

template <class T>
struct identity_step {
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T>
struct limit_step {
  size_t remaining;

  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (remaining > 0) {
      if (next.on_next(item, steps...)) {
        if (--remaining > 0) {
          return true;
        } else {
          next.on_complete(steps...);
          return false;
        }
      }
    }
    return false;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Predicate>
struct filter_step {
  using trait = detail::get_callable_trait_t<Predicate>;

  static_assert(std::is_convertible_v<typename trait::result_type, bool>,
                "predicates must return a boolean value");

  static_assert(trait::num_args == 1,
                "predicates must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = input_type;

  Predicate predicate;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (predicate(item))
      return next.on_next(item, steps...);
    else
      return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Predicate>
struct take_while_step {
  using trait = detail::get_callable_trait_t<Predicate>;

  static_assert(std::is_convertible_v<typename trait::result_type, bool>,
                "predicates must return a boolean value");

  static_assert(trait::num_args == 1,
                "predicates must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = input_type;

  Predicate predicate;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (predicate(item)) {
      return next.on_next(item, steps...);
    } else {
      next.on_complete(steps...);
      return false;
    }
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T>
struct distinct_step {
  using input_type = T;

  using output_type = T;

  std::unordered_set<T> prev;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (prev.insert(item).second)
      return next.on_next(item, steps...);
    else
      return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class Fn>
struct map_step {
  using trait = detail::get_callable_trait_t<Fn>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "map functions may not return void");

  static_assert(trait::num_args == 1,
                "map functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using output_type = std::decay_t<typename trait::result_type>;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(fn(item), steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Reducer>
struct reduce_step {
  T result;

  Reducer fn;

  reduce_step(T init, Reducer reducer) : result(std::move(init)), fn(reducer) {
    // nop
  }

  reduce_step(reduce_step&&) = default;
  reduce_step(const reduce_step&) = default;
  reduce_step& operator=(reduce_step&&) = default;
  reduce_step& operator=(const reduce_step&) = default;

  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next&, Steps&...) {
    result = fn(std::move(result), item);
    return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    if (next.on_next(result, steps...))
      next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    if (next.on_next(result, steps...))
      next.on_error(what, steps...);
  }
};

template <class Fn>
struct flat_map_optional_step {
  using trait = detail::get_callable_trait_t<Fn>;

  static_assert(!std::is_same_v<typename trait::result_type, void>,
                "flat_map_optional functions may not return void");

  static_assert(trait::num_args == 1,
                "flat_map_optional functions must take exactly one argument");

  using input_type = std::decay_t<detail::tl_head_t<typename trait::arg_types>>;

  using intermediate_type = std::decay_t<typename trait::result_type>;

  using output_type = typename intermediate_type::value_type;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    if (auto val = fn(item))
      return next.on_next(*val, steps...);
    else
      return true;
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_on_next_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    fn(item);
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_on_complete_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    fn();
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_on_error_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    fn(what);
    next.on_error(what, steps...);
  }
};

template <class T, class Fn>
struct do_finally_step {
  using input_type = T;

  using output_type = T;

  Fn fn;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    fn();
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error& what, Next& next, Steps&... steps) {
    fn();
    next.on_error(what, steps...);
  }
};

/// Catches errors by converting them into `complete` events instead.
template <class T>
struct on_error_complete_step {
  using input_type = T;

  using output_type = T;

  template <class Next, class... Steps>
  bool on_next(const input_type& item, Next& next, Steps&... steps) {
    return next.on_next(item, steps...);
  }

  template <class Next, class... Steps>
  void on_complete(Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }

  template <class Next, class... Steps>
  void on_error(const error&, Next& next, Steps&... steps) {
    next.on_complete(steps...);
  }
};

/// Utility for the observables that use one or more steps.
template <class... Steps>
struct steps_output_oracle;

template <class Step>
struct steps_output_oracle<Step> {
  using type = typename Step::output_type;
};

template <class Step1, class Step2, class... Steps>
struct steps_output_oracle<Step1, Step2, Steps...>
  : steps_output_oracle<Step2, Steps...> {};

template <class... Steps>
using steps_output_type_t = typename steps_output_oracle<Steps...>::type;

} // namespace caf::flow
