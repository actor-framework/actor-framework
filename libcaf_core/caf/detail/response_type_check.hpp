// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/callable_trait.hpp"
#include "caf/fwd.hpp"
#include "caf/policy/select_all_tag.hpp"
#include "caf/policy/select_any_tag.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

template <class OnError>
constexpr void on_error_type_check() {
  // Type-check OnError.
  using on_error_trait_helper = detail::get_callable_trait_helper<OnError>;
  static_assert(on_error_trait_helper::valid,
                "OnError must provide a single, non-template operator()");
  using on_error_trait = typename on_error_trait_helper::type;
  static_assert(std::is_same_v<typename on_error_trait::result_type, void>,
                "OnError must return void");
  using on_error_args = typename on_error_trait::decayed_arg_types;
  static_assert(std::is_same_v<on_error_args, type_list<error>>,
                "OnError must accept a single argument of type caf::error");
}

template <class OnValue, class OnError, class... Results>
constexpr void response_type_check() {
  using res_t = type_list<Results...>;
  // Type-check OnValue.
  using on_value_trait_helper = detail::get_callable_trait_helper<OnValue>;
  static_assert(on_value_trait_helper::valid,
                "OnValue must provide a single, non-template operator()");
  using on_value_trait = typename on_value_trait_helper::type;
  static_assert(std::is_same_v<typename on_value_trait::result_type, void>,
                "OnValue must return void");
  if constexpr (!std::is_same_v<res_t, type_list<message>>) {
    using on_value_args = typename on_value_trait::decayed_arg_types;
    static_assert(std::is_same_v<on_value_args, res_t>,
                  "OnValue does not match the expected response types");
  }
  on_error_type_check<OnError>();
}

/// Policy-aware type checking for fan-out response handles.
template <class Policy, class OnValue, class OnError, class... Results>
constexpr void fan_out_response_type_check() {
  // Type check for OnValue - select all policy.
  if constexpr (std::is_same_v<Policy, policy::select_all_tag_t>) {
    using res_t = type_list<Results...>;
    using on_value_trait_helper = detail::get_callable_trait_helper<OnValue>;
    static_assert(on_value_trait_helper::valid,
                  "OnValue must provide a single, non-template operator()");
    using on_value_trait = typename on_value_trait_helper::type;
    static_assert(std::is_same_v<typename on_value_trait::result_type, void>,
                  "OnValue must return void");
    if constexpr (!std::is_same_v<res_t, type_list<message>>) {
      using on_value_args = typename on_value_trait::decayed_arg_types;
      if constexpr (sizeof...(Results) == 0) {
        static_assert(
          std::is_same_v<on_value_args, type_list<>>,
          "OnValue for select_all with void results must take no arguments");
      } else if constexpr (sizeof...(Results) == 1) {
        using expected_type
          = type_list<std::vector<typename detail::tl_head<res_t>::type>>;
        static_assert(std::is_same_v<on_value_args, expected_type>,
                      "OnValue for select_all with single result type must "
                      "accept std::vector<Result>");
      } else {
        using expected_type = type_list<std::vector<std::tuple<Results...>>>;
        static_assert(std::is_same_v<on_value_args, expected_type>,
                      "OnValue for select_all with multiple result types must "
                      "accept std::vector<std::tuple<Results...>>");
      }
    }
    on_error_type_check<OnError>();
  } else {
    static_assert(std::is_same_v<Policy, policy::select_any_tag_t>);
    // Select any fan out request has the same signatures as a regular request.
    response_type_check<OnValue, OnError, Results...>();
  }
}

} // namespace caf::detail
