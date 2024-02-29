// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"
#include "caf/type_list.hpp"

#include <type_traits>

namespace caf::detail {

template <class OnValue, class OnError, class... Results>
constexpr void response_type_check() {
  using res_t = type_list<Results...>;
  // Type-check OnValue.
  using on_value_trait_helper = typename detail::get_callable_trait<OnValue>;
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
  // Type-check OnError.
  using on_error_trait_helper = typename detail::get_callable_trait<OnError>;
  static_assert(on_error_trait_helper::valid,
                "OnError must provide a single, non-template operator()");
  using on_error_trait = typename on_error_trait_helper::type;
  static_assert(std::is_same_v<typename on_error_trait::result_type, void>,
                "OnError must return void");
  using on_error_args = typename on_error_trait::decayed_arg_types;
  static_assert(std::is_same_v<on_error_args, type_list<error>>,
                "OnError must accept a single argument of type caf::error");
}

} // namespace caf::detail
