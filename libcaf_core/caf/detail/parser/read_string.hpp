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
#include <string>

#include "caf/detail/scope_guard.hpp"

#include "caf/detail/parser/fsm.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/pec.hpp"

namespace caf {
namespace detail {
namespace parser {

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class Iterator, class Sentinel, class Consumer>
void read_string(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  std::string res;
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(std::move(res));
  });
  start();
  state(init) {
    transition(init, " \t")
    transition(read_chars, '"')
  }
  state(read_chars) {
    transition(escape, '\\')
    transition(done, '"')
    error_transition(pec::unexpected_newline, '\n')
    transition(read_chars, any_char, res += ch)
  }
  state(escape) {
    transition(read_chars, 'n', res += '\n')
    transition(read_chars, 'r', res += '\r')
    transition(read_chars, 't', res += '\t')
    transition(read_chars, '\\', res += '\\')
    transition(read_chars, '"', res += '"')
    error_transition(pec::illegal_escape_sequence)
  }
  term_state(done) {
    transition(done, " \t")
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

