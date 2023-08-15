// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"

#include <string>

namespace caf::test {

/// Thrown when a requirement check fails. When `CAF_ENABLE_EXCEPTIONS` is off,
/// the `raise` functions terminate the program instead.
class CAF_TEST_EXPORT requirement_error {
public:
  constexpr requirement_error(const requirement_error&) noexcept = default;

  constexpr requirement_error& operator=(const requirement_error&) noexcept
    = default;

  /// Returns a human-readable error message.
  std::string message() const;

  /// Returns the source location of the error.
  constexpr const detail::source_location& location() const noexcept {
    return loc_;
  }

  /// Throws a `requirement_error` to indicate that requirement check failed.
  [[noreturn]] static void raise(const detail::source_location& loc
                                 = detail::source_location::current()) {
    raise_impl(loc);
  }

private:
  constexpr explicit requirement_error(
    const detail::source_location& loc) noexcept
    : loc_(loc) {
    // nop
  }

  [[noreturn]] static void raise_impl(const detail::source_location& loc);

  detail::source_location loc_;
};

} // namespace caf::test
