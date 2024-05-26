// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include <string_view>

namespace caf::test {

/// Describes a binary predicate.
enum class binary_predicate {
  /// Equal.
  eq,
  /// Not equal.
  ne,
  /// Less than.
  lt,
  /// Less than or equal.
  le,
  /// Greater than.
  gt,
  /// Greater than or equal.
  ge,
};

/// Returns a string representation of `predicate` in C++ syntax.
constexpr std::string_view str(binary_predicate predicate) noexcept {
  switch (predicate) {
    default:
      return "???";
    case binary_predicate::eq:
      return "==";
    case binary_predicate::ne:
      return "!=";
    case binary_predicate::lt:
      return "<";
    case binary_predicate::le:
      return "<=";
    case binary_predicate::gt:
      return ">";
    case binary_predicate::ge:
      return ">=";
  }
}

/// Returns the negated predicate.
constexpr binary_predicate negate(binary_predicate predicate) noexcept {
  switch (predicate) {
    default:
      return predicate;
    case binary_predicate::eq:
      return binary_predicate::ne;
    case binary_predicate::ne:
      return binary_predicate::eq;
    case binary_predicate::lt:
      return binary_predicate::ge;
    case binary_predicate::le:
      return binary_predicate::gt;
    case binary_predicate::gt:
      return binary_predicate::le;
    case binary_predicate::ge:
      return binary_predicate::lt;
  }
}

} // namespace caf::test
