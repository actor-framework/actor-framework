// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/assert.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_number.hpp"
#include "caf/detail/parser/read_timespan.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/none.hpp"
#include "caf/pec.hpp"
#include "caf/timestamp.hpp"

#include <chrono>
#include <cstdint>
#include <string>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a number or a duration, i.e., on success produces an `int64_t`, a
/// `double`, or a `timespan`.
template <class State, class Consumer, class EnableRange = std::false_type>
void read_number_or_timespan(State& ps, Consumer& consumer,
                             EnableRange enable_range = {}) {
  using namespace std::chrono;
  struct interim_consumer {
    size_t invocations = 0;
    Consumer* outer = nullptr;
    std::variant<none_t, int64_t, double> interim;
    void value(int64_t x) {
      // If we see a second integer, we have a range of integers and forward all
      // calls to the outer consumer.
      switch (++invocations) {
        case 1:
          interim = x;
          break;
        case 2:
          CAF_ASSERT(std::holds_alternative<int64_t>(interim));
          outer->value(std::get<int64_t>(interim));
          interim = none;
          [[fallthrough]];
        default:
          outer->value(x);
      }
    }
    pec value(uint64_t x) {
      if (x <= INT64_MAX) {
        value(static_cast<int64_t>(x));
        return pec::success;
      }
      return pec::integer_overflow;
    }
    void value(double x) {
      interim = x;
    }
  };
  interim_consumer ic;
  ic.outer = &consumer;
  auto has_int = [&] { return std::holds_alternative<int64_t>(ic.interim); };
  auto has_dbl = [&] { return std::holds_alternative<double>(ic.interim); };
  auto get_int = [&] { return std::get<int64_t>(ic.interim); };
  auto disabled = false;
  constexpr auto enable_float = std::true_type{};
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_number(ps, ic, enable_float, enable_range), has_number)
  }
  term_state(has_number) {
    epsilon_if(has_int(), has_integer)
    epsilon_if(has_dbl(), has_double)
  }
  term_state(has_double) {
    error_transition(pec::fractional_timespan, "unmsh")
  }
  term_state(has_integer) {
    fsm_epsilon(read_timespan(ps, consumer, get_int()),
                done, "unmsh", disabled = true)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (!disabled && ps.code <= pec::trailing_character) {
    if (has_dbl())
      consumer.value(std::get<double>(ic.interim));
    else if (has_int())
      consumer.value(get_int());
  }
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
