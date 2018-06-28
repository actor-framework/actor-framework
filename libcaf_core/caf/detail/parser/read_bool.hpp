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

/// Reads a boolean.
template <class Iterator, class Sentinel, class Consumer>
void read_bool(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  bool res = false;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(res);
  });
  start();
  state(init) {
    transition(has_f, 'f')
    transition(has_t, 't')
  }
  state(has_f) {
    transition(has_fa, 'a')
  }
  state(has_fa) {
    transition(has_fal, 'l')
  }
  state(has_fal) {
    transition(has_fals, 's')
  }
  state(has_fals) {
    transition(done, 'e', res = false)
  }
  state(has_t) {
    transition(has_tr, 'r')
  }
  state(has_tr) {
    transition(has_tru, 'u')
  }
  state(has_tru) {
    transition(done, 'e', res = true)
  }
  term_state(done) {
    // nop
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
