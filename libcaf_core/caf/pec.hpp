// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/default_enum_inspect.hpp"
#include "caf/detail/core_export.hpp"
#include "caf/fwd.hpp"
#include "caf/is_error_code_enum.hpp"

#include <cstdint>
#include <string>
#include <type_traits>

namespace caf {

/// PEC stands for "Parser Error Code". This enum contains error codes used by
/// various CAF parsers.
enum class pec : uint8_t {
  /// Not-an-error.
  success = 0,
  /// Parser succeeded but found trailing character(s).
  trailing_character = 1,
  /// Parser stopped after reaching the end while still expecting input.
  unexpected_eof,
  /// Parser stopped after reading an unexpected character.
  unexpected_character,
  /// Parsed integer exceeds the number of available bits of a `timespan`.
  timespan_overflow,
  /// Tried constructing a `timespan` with from a floating point number.
  fractional_timespan = 5,
  /// Too many characters for an atom.
  too_many_characters,
  /// Unrecognized character after escaping `\`.
  invalid_escape_sequence,
  /// Misplaced newline, e.g., inside a string.
  unexpected_newline,
  /// Parsed positive integer exceeds the number of available bits.
  integer_overflow,
  /// Parsed negative integer exceeds the number of available bits.
  integer_underflow = 10,
  /// Exponent of parsed double is less than the minimum supported exponent.
  exponent_underflow,
  /// Exponent of parsed double is greater than the maximum supported exponent.
  exponent_overflow,
  /// Parsed type does not match the expected type.
  type_mismatch,
  /// Stopped at an unrecognized option name.
  not_an_option,
  /// Stopped at an unparsable argument.
  invalid_argument = 15,
  /// Stopped because an argument was omitted.
  missing_argument,
  /// Stopped because the key of a category was taken.
  invalid_category,
  /// Stopped at an unexpected field name while reading a user-defined type.
  invalid_field_name,
  /// Stopped at a repeated field name while reading a user-defined type.
  repeated_field_name,
  /// Stopped while reading a user-defined type with one or more missing
  /// mandatory fields.
  missing_field = 20,
  /// Parsing a range statement ('n..m' or 'n..m..step') failed.
  invalid_range_expression,
  /// Stopped after running into an invalid parser state. Should never happen
  /// and most likely indicates a bug in the implementation.
  invalid_state,
  /// Parser stopped after exceeding its maximum supported level of nesting.
  nested_too_deeply,
};

/// @relates pec
CAF_CORE_EXPORT std::string to_string(pec);

/// @relates pec
CAF_CORE_EXPORT bool from_string(std::string_view, pec&);

/// @relates pec
CAF_CORE_EXPORT bool from_integer(std::underlying_type_t<pec>, pec&);

/// @relates pec
template <class Inspector>
bool inspect(Inspector& f, pec& x) {
  return default_enum_inspect(f, x);
}

} // namespace caf

CAF_ERROR_CODE_ENUM(pec)
