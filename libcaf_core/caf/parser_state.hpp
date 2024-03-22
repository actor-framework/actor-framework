// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/detail/core_export.hpp"
#include "caf/error.hpp"
#include "caf/fwd.hpp"
#include "caf/pec.hpp"

#include <cctype>
#include <cstdint>
#include <string_view>

namespace caf {

/// Converts the code and the current position of a parser to an error.
CAF_CORE_EXPORT error parser_state_to_error(pec code, int32_t line,
                                            int32_t column);

/// Stores all information necessary for implementing an FSM-based parser.
template <class Iterator, class Sentinel>
struct parser_state {
  using iterator_type = Iterator;

  /// Current position of the parser.
  Iterator i;

  /// End-of-input marker.
  Sentinel e;

  /// Current state of the parser.
  pec code;

  /// Current line in the input.
  int32_t line;

  /// Position in the current line.
  int32_t column;

  parser_state() noexcept : i(), e(), code(pec::success), line(1), column(1) {
    // nop
  }

  explicit parser_state(Iterator first) noexcept : parser_state() {
    i = first;
  }

  parser_state(Iterator first, Sentinel last) noexcept : parser_state() {
    i = first;
    e = last;
  }

  parser_state(const parser_state&) noexcept = default;

  parser_state& operator=(const parser_state&) noexcept = default;

  /// Returns the null terminator when reaching the end of the string,
  /// otherwise the next character.
  char next() noexcept {
    ++i;
    ++column;
    if (i != e) {
      auto c = *i;
      if (c == '\n') {
        ++line;
        column = 1;
      }
      return c;
    }
    return '\0';
  }

  /// Returns the null terminator if `i == e`, otherwise the current character.
  char current() const noexcept {
    return i != e ? *i : '\0';
  }

  /// Checks whether `i == e`.
  bool at_end() const noexcept {
    return i == e;
  }

  /// Skips any whitespaces characters in the input.
  void skip_whitespaces() noexcept {
    auto c = current();
    while (isspace(c))
      c = next();
  }

  /// Tries to read `x` as the next character, automatically skipping leading
  /// whitespaces.
  bool consume(char x) noexcept {
    skip_whitespaces();
    if (current() == x) {
      next();
      return true;
    }
    return false;
  }

  /// Consumes the next character if it satisfies given predicate, automatically
  /// skipping leading whitespaces.
  template <class Predicate>
  bool consume_if(Predicate predicate) noexcept {
    skip_whitespaces();
    if (predicate(current())) {
      next();
      return true;
    }
    return false;
  }

  /// Tries to read `x` as the next character without automatically skipping
  /// leading whitespaces.
  bool consume_strict(char x) noexcept {
    if (current() == x) {
      next();
      return true;
    }
    return false;
  }

  /// Consumes the next character if it satisfies given predicate without
  /// automatically skipping leading whitespaces.
  template <class Predicate>
  bool consume_strict_if(Predicate predicate) noexcept {
    if (predicate(current())) {
      next();
      return true;
    }
    return false;
  }

  /// Returns an error from the current state.
  auto error() {
    return parser_state_to_error(code, line, column);
  }
};

/// Specialization for parsers operating on string views.
using string_parser_state = parser_state<std::string_view::iterator>;

} // namespace caf
