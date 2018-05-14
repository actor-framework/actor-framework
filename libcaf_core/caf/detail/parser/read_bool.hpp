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
void read_bool(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  bool res = false;
  auto g = make_scope_guard([&] {
    if (ps.code <= ec::trailing_character)
      consumer.value(res);
  });
  start();
  state(init) {
    input(is_char<' '>, init)
    input(is_char<'\t'>, init)
    input(is_char<'f'>, has_f)
    input(is_char<'t'>, has_t)
  }
  state(has_f) {
    input(is_char<'a'>, has_fa)
  }
  state(has_fa) {
    input(is_char<'l'>, has_fal)
  }
  state(has_fal) {
    input(is_char<'s'>, has_fals)
  }
  state(has_fals) {
    action(is_char<'e'>, done, res = false)
  }
  state(has_t) {
    input(is_char<'r'>, has_tr)
  }
  state(has_tr) {
    input(is_char<'u'>, has_tru)
  }
  state(has_tru) {
    action(is_char<'e'>, done, res = true)
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

