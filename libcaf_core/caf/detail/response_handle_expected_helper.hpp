// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/error.hpp"
#include "caf/expected.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

// The fallback implementation: OnValue does not accept an expected<T>.
template <class Result, class OnValue>
struct response_handle_expected_helper {
  static constexpr bool use_expected = false;
};

// The fallback implementation for statically typed response handles : OnValue
// does not accept an expected<T>.
template <class OnValue, bool IsEnabled, class... Args>
struct response_handle_expected_helper_impl {
  static constexpr bool use_expected = false;
};

template <class... Args>
struct response_handle_expected_helper_arg;

template <>
struct response_handle_expected_helper_arg<void> {
  using type = expected<void>;
  static type lift() {
    return {};
  }
};

template <class Arg>
struct response_handle_expected_helper_arg<Arg> {
  using type = expected<Arg>;

  static type lift(Arg&& arg) {
    return type{std::move(arg)};
  }
};

template <class Arg1, class Arg2, class... Args>
struct response_handle_expected_helper_arg<Arg1, Arg2, Args...> {
  using type = expected<std::tuple<Arg1, Arg2, Args...>>;

  static type lift(Arg1&& arg1, Arg2&& arg2, Args&&... args) {
    return type{
      std::tuple{std::move(arg1), std::move(arg2), std::move(args)...}};
  }
};

template <class... Args>
using response_handle_expected_helper_arg_t =
  typename response_handle_expected_helper_arg<Args...>::type;

// The implementation for statically typed response handles where OnValue
// accepts an expected<T>.
template <class OnValue, class... Args>
struct response_handle_expected_helper_impl<OnValue, true, Args...> {
  static constexpr bool use_expected = true;

  static auto on_value(OnValue on_value) {
    return [fn = std::move(on_value)](Args... args) mutable {
      using helper = response_handle_expected_helper_arg<Args...>;
      return fn(helper::lift(std::move(args)...));
    };
  }

  static auto on_error(OnValue on_value) {
    return [fn = std::move(on_value)](error err) mutable {
      using expected_t = response_handle_expected_helper_arg_t<Args...>;
      return fn(expected_t{std::move(err)});
    };
  }
};

template <class OnValue>
struct response_handle_expected_helper_impl<OnValue, true, void> {
  static constexpr bool use_expected = true;

  static auto on_value(OnValue on_value) {
    return [fn = std::move(on_value)]() mutable {
      return fn(expected<void>{});
    };
  }

  static auto on_error(OnValue on_value) {
    return [fn = std::move(on_value)](error err) mutable {
      return fn(expected<void>{std::move(err)});
    };
  }
};

// The implementation for statically typed response handles.
template <class... Args, class OnValue>
struct response_handle_expected_helper<type_list<Args...>, OnValue>
  : response_handle_expected_helper_impl<
      OnValue,
      std::is_invocable_v<OnValue,
                          response_handle_expected_helper_arg_t<Args...>>,
      Args...> {};

// The fallback implementation for dynamically typed response handles : OnValue
// does not accept an expected<T>.
template <class DecayedArgs>
struct response_handle_expected_helper_dyn_trait {
  static constexpr bool use_expected = false;
};

// The implementation for dynamically typed response handles where OnValue
// accepts an expected<T>.
template <class T>
struct response_handle_expected_helper_dyn_trait<
  type_list<expected<T>>> {
  static constexpr bool use_expected = true;
  using value_type = T;
};

// Fallback in case that get_callable_trait_t<OnValue>::decayed_arg_types does
// not contain an expected<T>.
template <class OnValue, class Args>
struct response_handle_expected_helper_dyn_base {
  struct type {
    static constexpr bool use_expected = false;
  };
};

// Utility for flattening a std::tuple.
template <class OnValue, class Arg>
struct response_handle_expected_helper_dyn_base_flat {
  using type = response_handle_expected_helper_impl<OnValue, true, Arg>;
};

template <class OnValue, class... Args>
struct response_handle_expected_helper_dyn_base_flat<OnValue,
                                                     std::tuple<Args...>> {
  using type = response_handle_expected_helper_impl<OnValue, true, Args...>;
};

// The implementation for dynamically typed response handles where OnValue
// accepts an expected<T>.
template <class OnValue, class Arg>
struct response_handle_expected_helper_dyn_base<OnValue,
                                                type_list<expected<Arg>>> {
  using type =
    typename response_handle_expected_helper_dyn_base_flat<OnValue, Arg>::type;
};

// Utility for response_handle_expected_helper_dyn_base.
template <class OnValue, class Trait = get_callable_trait_t<OnValue>>
using response_handle_expected_helper_dyn_base_t =
  typename response_handle_expected_helper_dyn_base<
    OnValue, typename Trait::decayed_arg_types>::type;

// The implementation for dynamically typed response handles.
template <class OnValue>
struct response_handle_expected_helper<message, OnValue>
  : response_handle_expected_helper_dyn_base_t<OnValue> {};

} // namespace caf::detail
