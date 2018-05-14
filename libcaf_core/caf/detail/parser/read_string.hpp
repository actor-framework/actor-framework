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
void read_string(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  std::string res;
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= ec::trailing_character)
      consumer.value(std::move(res));
  });
  start();
  state(init) {
    input(is_char<' '>, init)
    input(is_char<'\t'>, init)
    input(is_char<'"'>, read_chars)
  }
  state(read_chars) {
    input(is_char<'\\'>, escape)
    input(is_char<'"'>, done)
    invalid_input(is_char<'\n'>, ec::unexpected_newline)
    default_action(read_chars, res += ch)
  }
  state(escape) {
    action(is_char<'n'>, read_chars, res += '\n')
    action(is_char<'r'>, read_chars, res += '\r')
    action(is_char<'t'>, read_chars, res += '\t')
    action(is_char<'\\'>, read_chars, res += '\\')
    action(is_char<'"'>, read_chars, res += '"')
    default_failure(ec::illegal_escape_sequence)
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

