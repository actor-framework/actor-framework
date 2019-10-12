/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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

#include <cctype>
#include <cstdint>

#include "caf/fwd.hpp"
#include "caf/pec.hpp"
#include "caf/string_view.hpp"

namespace caf {

/// Stores all informations necessary for implementing an FSM-based parser.
template <class Iterator, class Sentinel>
struct parser_state {
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

  /// Tries to read `x` as the next character (skips any whitespaces).
  bool consume(char x) noexcept {
    skip_whitespaces();
    if (current() == x) {
      next();
      return true;
    }
    return false;
  }
};

/// Returns an error object from the current code in `ps` as well as its
/// current position.
template <class Iterator, class Sentinel>
auto make_error(const parser_state<Iterator, Sentinel>& ps)
  -> decltype(make_error(ps.code, ps.line, ps.column)) {
  return make_error(ps.code, ps.line, ps.column);
}

/// Specialization for parsers operating on string views.
using string_parser_state = parser_state<string_view::iterator>;

} // namespace caf
