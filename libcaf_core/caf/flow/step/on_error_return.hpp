// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/type_traits.hpp"
#include "caf/fwd.hpp"

#include <utility>

namespace caf::flow::step {

template <class ErrorHandler>
class on_error_return {
public:
  using trait = detail::get_callable_trait_t<ErrorHandler>;

  static_assert(trait::num_args == 1, "error_handler must take an argument");

  using handler_res
    = decltype(std::declval<ErrorHandler&>()(std::declval<const error&>()));

  static_assert(detail::is_expected_v<handler_res>,
                "error_handler must return a caf::expected type");

  using input_type = typename handler_res::value_type;

  using output_type = input_type;

  explicit on_error_return(ErrorHandler fn) : fn_(std::move(fn)) {
    // nop
  }

  on_error_return(on_error_return&&) = default;
  on_error_return(const on_error_return&) = default;
  on_error_return& operator=(on_error_return&&) = default;
  on_error_return& operator=(const on_error_return&) = default;

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
    if (auto result = fn_(what)) {
      if (next.on_next(std::move(*result), steps...))
        next.on_complete(steps...);
    } else
      next.on_error(result.error(), steps...);
  }

private:
  ErrorHandler fn_;
};

} // namespace caf::flow::step
