// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

#include <type_traits>

namespace caf::async {

/// Encodes the result of an asynchronous read operation.
enum class read_result {
  /// Signals that the read operation succeeded.
  ok,
  /// Signals that the reader reached the end of the input.
  stop,
  /// Signals that the source failed with an error.
  abort,
  /// Signals that the read operation timed out.
  timeout,
};

/// @relates read_result
CAF_CORE_EXPORT std::string to_string(read_result);

/// @relates read_result
CAF_CORE_EXPORT bool from_string(string_view, read_result&);

/// @relates read_result
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<read_result>,
                                  read_result&);

/// @relates read_result
template <class Inspector>
bool inspect(Inspector& f, read_result& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf::async
