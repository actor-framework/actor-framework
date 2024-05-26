// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"

#include <chrono>
#include <tuple>
#include <type_traits>

namespace caf {

class CAF_CORE_EXPORT timeout_definition_builder {
public:
  explicit constexpr timeout_definition_builder(timespan d) : tout_(d) {
    // nop
  }

  template <class F>
  timeout_definition<F> operator>>(F f) const {
    return {tout_, std::move(f)};
  }

private:
  timespan tout_;
};

/// Returns a generator for timeouts.
template <class Rep, class Period>
constexpr auto after(std::chrono::duration<Rep, Period> d) {
  using std::chrono::duration_cast;
  return timeout_definition_builder{duration_cast<timespan>(d)};
}

} // namespace caf
