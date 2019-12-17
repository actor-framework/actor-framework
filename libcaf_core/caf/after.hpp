/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#pragma once

#include <chrono>
#include <tuple>
#include <type_traits>

#include "caf/detail/core_export.hpp"
#include "caf/timeout_definition.hpp"
#include "caf/timespan.hpp"

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
