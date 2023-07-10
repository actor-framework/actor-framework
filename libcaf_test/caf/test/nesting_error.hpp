// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/source_location.hpp"
#include "caf/detail/test_export.hpp"
#include "caf/test/block_type.hpp"

namespace caf::test {

/// Thrown when a block is opened in an invalid parent. When
/// `CAF_ENABLE_EXCEPTIONS` is off, the `raise` functions terminate the program
/// instead.
class CAF_TEST_EXPORT nesting_error {
public:
  constexpr nesting_error(const nesting_error&) noexcept = default;

  constexpr nesting_error& operator=(const nesting_error&) noexcept = default;

  /// Returns a human-readable error message.
  std::string message() const;

  /// Returns the type of the parent in which the error occurred.
  constexpr block_type parent() const noexcept {
    return parent_;
  }

  /// Returns the type of the block that caused the error.
  constexpr block_type child() const noexcept {
    return child_;
  }

  /// Returns the source location of the error.
  constexpr const detail::source_location& location() const noexcept {
    return loc_;
  }

  /// Throws a `nesting_error` to indicate that `child` is not allowed in
  /// `parent`.
  [[noreturn]] static void
  raise_not_allowed(block_type parent, block_type child,
                    const detail::source_location& loc
                    = detail::source_location::current()) {
    raise_impl(code::not_allowed, parent, child, loc);
  }

  /// Throws a `nesting_error` to indicate that `parent` is not allowing
  /// additional blocks of type `child`.
  [[noreturn]] static void
  raise_too_many(block_type parent, block_type child,
                 const detail::source_location& loc
                 = detail::source_location::current()) {
    raise_impl(code::too_many, parent, child, loc);
  }

  /// Throws a `nesting_error` to indicate that `child` expected a `parent`
  /// block prior to it.
  [[noreturn]] static void
  raise_invalid_sequence(block_type parent, block_type child,
                         const detail::source_location& loc
                         = detail::source_location::current()) {
    raise_impl(code::invalid_sequence, parent, child, loc);
  }

private:
  enum class code {
    not_allowed,
    too_many,
    invalid_sequence,
  };

  constexpr nesting_error(code what, block_type parent, block_type child,
                          const detail::source_location& loc) noexcept
    : code_(what), parent_(parent), child_(child), loc_(loc) {
    // nop
  }

  [[noreturn]] static void raise_impl(code what, block_type parent,
                                      block_type child,
                                      const detail::source_location& loc);

  code code_;
  block_type parent_;
  block_type child_;
  detail::source_location loc_;
};

} // namespace caf::test
