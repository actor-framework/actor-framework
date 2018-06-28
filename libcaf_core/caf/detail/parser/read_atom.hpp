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
#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

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
    if (ps.code <= pec::trailing_character)
      consumer.value(atom(buf));
  });
  start();
  state(init) {
    transition(init, " \t")
    transition(read_chars, '\'')
  }
  state(read_chars) {
    transition(done, '\'')
    transition(read_chars, is_legal, append(ch), pec::too_many_characters)
  }
  term_state(done) {
    transition(done, " \t")
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
