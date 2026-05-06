// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

#include <string_view>
#include <type_traits>

namespace caf::async {

/// Encodes the result of a write into an asynchronous data sink.
enum class write_result {
  /// The sink accepted all data.
  ok,
  /// The sink rejected some or all of the data but may accept more in the
  /// future.
  try_again_later,
  /// The sink no longer accepts any data.
  canceled,
};

/// @relates write_result
CAF_CORE_EXPORT std::string to_string(write_result);

/// @relates write_result
CAF_CORE_EXPORT bool from_string(std::string_view, write_result&);

/// @relates write_result
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<write_result>,
                                  write_result&);

/// @relates write_result
template <class Inspector>
bool inspect(Inspector& f, write_result& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::async
