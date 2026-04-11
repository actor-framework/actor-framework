// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <source_location>
#include <string_view>

namespace caf {

/// Wraps a format string and its source location. Useful for logging functions
/// that have a variadic list of arguments and thus cannot use the usual way of
/// passing in a source location via default argument.
struct format_string_with_location {
  constexpr format_string_with_location(std::string_view str,
                                        const std::source_location& loc
                                        = std::source_location::current())
    : value(str), location(loc) {
    // nop
  }

  constexpr format_string_with_location(const char* str,
                                        const std::source_location& loc
                                        = std::source_location::current())
    : value(str), location(loc) {
    // nop
  }

  /// The format string.
  std::string_view value;

  /// The source location of the format string.
  std::source_location location;
};

} // namespace caf
