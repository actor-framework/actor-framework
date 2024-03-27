// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf::flow {

/// Selects a strategy for handling backpressure.
enum class backpressure_overflow_strategy {
  /// Drops the newest item when the buffer is full.
  drop_newest,
  /// Drops the oldest item when the buffer is full.
  drop_oldest,
  /// Raises an error when the buffer is full.
  fail
};

/// @relates backpressure_overflow_strategy
CAF_CORE_EXPORT std::string to_string(backpressure_overflow_strategy);

/// @relates backpressure_overflow_strategy
CAF_CORE_EXPORT bool from_string(std::string_view,
                                 backpressure_overflow_strategy&);

/// @relates backpressure_overflow_strategy
CAF_CORE_EXPORT bool
from_integer(std::underlying_type_t<backpressure_overflow_strategy>,
             backpressure_overflow_strategy&);

/// @relates backpressure_overflow_strategy
template <class Inspector>
bool inspect(Inspector& f, backpressure_overflow_strategy& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::flow
