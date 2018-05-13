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

#include <cstdint>

namespace caf {
namespace detail {
namespace parser {

enum class ec : uint8_t {
  /// Not-an-error.
  success,
  /// Parser succeeded but found trailing character(s).
  trailing_character,
  /// Parser stopped after reaching the end while still expecting input.
  unexpected_eof,
  /// Parser stopped after reading an unexpected character.
  unexpected_character,
  /// Cannot construct a negative caf::duration.
  negative_duration,
  /// Cannot construct a caf::duration with a value exceeding uint32_t.
  duration_overflow,
  /// Too many characters for an atom.
  too_many_characters,
  /// Unrecognized character after escaping `\`.
  illegal_escape_sequence,
  /// Misplaced newline, e.g., inside a string.
  unexpected_newline,
  /// Parsed positive integer exceeds the number of available bits.
  integer_overflow,
  /// Parsed negative integer exceeds the number of available bits.
  integer_underflow,
  /// Exponent of parsed double is less than the minimum supported exponent.
  exponent_underflow,
  /// Exponent of parsed double is greater than the maximum supported exponent.
  exponent_overflow,
};

const char* to_string(ec x);

} // namespace parser
} // namespace detail
} // namespace caf
