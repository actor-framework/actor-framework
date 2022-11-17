// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"

#include <string_view>
#include <type_traits>

namespace caf::async {

/// Encodes the result of an asynchronous write operation.
enum class write_result {
  /// Signals that the write operation succeeded.
  ok,
  /// Signals that the item must be dropped because the write operation failed
  /// with an unrecoverable error. Retries will fail with the same result. When
  /// writing to a @ref producer_resource, this usually means the consumer
  /// closed its end of the buffer.
  drop,
  /// Signals that the write operation timed out.
  timeout,
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
