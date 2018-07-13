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

#include "caf/pec.hpp"

namespace caf {
namespace detail {
namespace parser {

template <class Iterator, class Sentinel = Iterator>
struct state {
  Iterator i;
  Sentinel e;
  pec code;
  int32_t line;
  int32_t column;

  state() : i(), e(), code(pec::success), line(1), column(1) {
    // nop
  }

  explicit state(Iterator first) : state() {
    i = first;
  }

  state(Iterator first, Sentinel last) : state() {
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
};

} // namespace parser
} // namespace detail
} // namespace caf
