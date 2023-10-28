// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/timestamp.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a timespan.
template <class State, class Consumer>
void read_timespan(State& ps, Consumer&& consumer,
                   std::optional<int64_t> num = std::nullopt) {
  using namespace std::chrono;
  struct interim_consumer {
    using value_type = int64_t;

    void value(value_type y) {
      x = y;
    }

    value_type x = 0;
  };
  interim_consumer ic;
  timespan result;
  // clang-format off
  start();
  state(init) {
    epsilon_if(num, has_integer, any_char, ic.x = *num)
    fsm_epsilon(read_signed_integer(ps, ic), has_integer)
  }
  state(has_integer) {
    transition(have_u, 'u')
    transition(have_n, 'n')
    transition(have_m, 'm')
    transition(done, 's', result = seconds(ic.x))
    transition(done, 'h', result = hours(ic.x))
  }
  state(have_u) {
    transition(done, 's', result = microseconds(ic.x))
  }
  state(have_n) {
    transition(done, 's', result = nanoseconds(ic.x))
  }
  state(have_m) {
    transition(have_mi, 'i')
    transition(done, 's', result = milliseconds(ic.x))
  }
  state(have_mi) {
    transition(done, 'n', result = minutes(ic.x))
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character)
    consumer.value(std::move(result));
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
