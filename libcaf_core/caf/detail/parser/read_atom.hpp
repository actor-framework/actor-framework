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
#include <ctype.h>
#include <string>

#include "caf/atom.hpp"

#include "caf/detail/scope_guard.hpp"

#include "caf/detail/parser/ec.hpp"
#include "caf/detail/parser/fsm.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/state.hpp"

namespace caf {
namespace detail {
namespace parser {

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class Iterator, class Sentinel, class Consumer>
void read_atom(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  size_t pos = 0;
  char buf[11];
  memset(buf, 0, sizeof(buf));
  auto is_legal = [](char c) {
    return isalnum(c) || c == '_' || c == ' ';
  };
  auto append = [&](char c) {
    if (pos == sizeof(buf) - 1)
      return false;
    buf[pos++] = c;
    return true;
  };
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= ec::trailing_character)
      consumer.value(atom(buf));
  });
  start();
  state(init) {
    input(is_char<' '>, init)
    input(is_char<'\t'>, init)
    input(is_char<'\''>, read_chars)
  }
  state(read_chars) {
    input(is_char<'\''>, done)
    checked_action(is_legal, read_chars, append(ch), ec::too_many_characters)
    invalid_input(is_char<'\n'>, ec::unexpected_newline)
  }
  term_state(done) {
    input(is_char<' '>, done)
    input(is_char<'\t'>, done)
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

