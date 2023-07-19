// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

#include <cstdint>
#include <string>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a boolean.
template <class State, class Consumer>
void read_bool(State& ps, Consumer&& consumer) {
  bool res = false;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(std::move(res));
  });
  // clang-format off
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
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
