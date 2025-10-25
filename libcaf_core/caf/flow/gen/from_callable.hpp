// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/concepts.hpp"
#include "caf/detail/metaprogramming.hpp"

#include <type_traits>

namespace caf::flow::gen {

/// A generator that emits values from a function object.
template <class F>
class from_callable {
public:
  using callable_res_t = std::invoke_result_t<F>;

  static constexpr bool boxed_output = detail::is_optional<callable_res_t>
                                       || detail::is_expected<callable_res_t>;

  using output_type = detail::unboxed<callable_res_t>;

  explicit from_callable(F fn) : fn_(std::move(fn)) {
    // nop
  }

  from_callable(from_callable&&) = default;
  from_callable(const from_callable&) = default;
  from_callable& operator=(from_callable&&) = default;
  from_callable& operator=(const from_callable&) = default;

  template <class Step, class... Steps>
  void pull(size_t n, Step& step, Steps&... steps) {
    for (size_t i = 0; i < n; ++i) {
      if constexpr (boxed_output) {
        auto val = fn_();
        if (!val) {
          if constexpr (detail::is_expected<callable_res_t>) {
            if (const auto& err = val.error())
              step.on_error(err, steps...);
            else
              step.on_complete(steps...);
          } else {
            step.on_complete(steps...);
          }
          return;
        }
        if (!step.on_next(*val, steps...))
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

} // namespace caf::flow::gen
