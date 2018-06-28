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
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_number.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/none.hpp"
#include "caf/optional.hpp"
#include "caf/pec.hpp"
#include "caf/timestamp.hpp"
#include "caf/variant.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf {
namespace detail {
namespace parser {

/// Reads a number or a duration, i.e., on success produces an `int64_t`, a
/// `double`, or a `timespan`.
template <class Iterator, class Sentinel, class Consumer>
void read_number_or_timespan(state<Iterator, Sentinel>& ps,
                             Consumer& consumer) {
  using namespace std::chrono;
  struct interim_consumer {
    variant<none_t, int64_t, double> interim;
    void value(int64_t x) {
      interim = x;
    }
    void value(double x) {
      interim = x;
    }
  };
  optional<timespan> res;
  interim_consumer ic;
  auto has_int = [&] { return holds_alternative<int64_t>(ic.interim); };
  auto has_dbl = [&] { return holds_alternative<double>(ic.interim); };
  auto get_int = [&] { return get<int64_t>(ic.interim); };
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      if (res != none) {
        consumer.value(*res);
      } else if (!holds_alternative<none_t>(ic.interim)) {
        if (has_int())
          consumer.value(get_int());
        else
          consumer.value(get<double>(ic.interim));
      }
    }
  });
  start();
  state(init) {
    fsm_epsilon(read_number(ps, ic), has_number)
  }
  term_state(has_number) {
    epsilon_if(has_int(), has_integer)
    epsilon_if(has_dbl(), has_double)
  }
  term_state(has_double) {
    error_transition(pec::fractional_timespan, "unms")
  }
  term_state(has_integer) {
    transition(have_u, 'u')
    transition(have_n, 'n')
    transition(have_m, 'm')
    transition(done, 's', res = seconds(get_int()))
  }
  state(have_u) {
    transition(done, 's', res = microseconds(get_int()))
  }
  state(have_n) {
    transition(done, 's', res = nanoseconds(get_int()))
  }
  state(have_m) {
    transition(have_mi, 'i')
    transition(done, 's', res = milliseconds(get_int()))
  }
  state(have_mi) {
    transition(done, 'n', res = minutes(get_int()))
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
