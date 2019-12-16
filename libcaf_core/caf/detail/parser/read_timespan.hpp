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

#include <chrono>
#include <cstdint>
#include <string>

#include "caf/config.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/optional.hpp"
#include "caf/pec.hpp"
#include "caf/timestamp.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a timespan.
template <class State, class Consumer>
void read_timespan(State& ps, Consumer&& consumer,
                   optional<int64_t> num = none) {
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
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(std::move(result));
  });
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
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
