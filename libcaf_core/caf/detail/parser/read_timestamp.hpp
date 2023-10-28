// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/chrono.hpp"
#include "caf/config.hpp"
#include "caf/detail/consumer.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/scope_guard.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

// Parses an integer in the form "00". We can't use `read_int` here because
// it will interpret the leading zero as an octal prefix.
template <class State, class Consumer>
void read_two_digit_int(State& ps, Consumer&& consumer) {
  auto result = 0;
  // clang-format off
  start();
  state(init) {
    transition(at_digit2, decimal_chars, add_ascii<10>(result, ch))
  }
  state(at_digit2) {
    transition(done, decimal_chars, add_ascii<10>(result, ch))
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character) {
    consumer.value(result);
  }
}

/// Reads a UTC offset in ISO 8601 format, i.e., a string of the form
/// `[+-]HH:MM`, `[+-]HHMM`, or `[+-]HH`.
template <class State, class Consumer>
void read_utc_offset(State& ps, Consumer&& consumer) {
  bool negative = false;
  auto hh = 0;
  auto mm = 0;
  // clang-format off
  start();
  state(init) {
    fsm_transition(read_two_digit_int(ps, make_consumer(hh)), after_hh, '+')
    fsm_transition(read_two_digit_int(ps, make_consumer(hh)), after_hh, '-',
                   negative = true)
  }
  term_state(after_hh) {
    fsm_transition(read_two_digit_int(ps, make_consumer(mm)), done, ':')
    fsm_epsilon(read_two_digit_int(ps, make_consumer(mm)), done, decimal_chars)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character) {
    auto result = hh * 3600 + mm * 60;
    consumer.value(negative ? -result : result);
  }
}

/// Reads a date and time in ISO 8601 format.
template <class State, class Consumer>
void read_timestamp(State& ps, Consumer&& consumer) {
  auto fractional_seconds = 0;
  auto fractional_seconds_decimals = 0;
  auto add_fractional = [&](char ch) {
    ++fractional_seconds_decimals;
    return add_ascii<10>(fractional_seconds, ch);
  };
  chrono::datetime result;
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_signed_integer(ps, make_consumer(result.year)),
                has_year, decimal_chars);
  }
  state(has_year) {
    fsm_transition(read_two_digit_int(ps, make_consumer(result.month)),
                   has_month, '-')
  }
  state(has_month) {
    fsm_transition(read_two_digit_int(ps, make_consumer(result.day)),
                   has_day, '-')
  }
  state(has_day) {
    transition(at_hour, 'T')
  }
  state(at_hour) {
    fsm_epsilon(read_two_digit_int(ps, make_consumer(result.hour)),
                has_hour, decimal_chars);
  }
  state(has_hour) {
    transition(at_minute, ':')
  }
  state(at_minute) {
    fsm_epsilon(read_two_digit_int(ps, make_consumer(result.minute)),
                has_minute, decimal_chars);
  }
  state(has_minute) {
    transition(at_second, ':')
  }
  state(at_second) {
    fsm_epsilon(read_two_digit_int(ps, make_consumer(result.second)),
                has_second, decimal_chars);
  }
  term_state(has_second) {
    transition(has_fractional, '.')
    transition(done, 'Z', result.utc_offset = 0)
    fsm_epsilon(read_utc_offset(ps, make_consumer(result.utc_offset.emplace(0))),
                done, "+-")
  }
  term_state(has_fractional) {
    transition_if(fractional_seconds_decimals < 9, has_fractional, decimal_chars,
                  add_fractional(ch))
    epsilon_if(fractional_seconds_decimals > 0, has_utc_offset)
  }
  term_state(has_utc_offset) {
    transition(done, 'Z', result.utc_offset = 0)
    fsm_epsilon(read_utc_offset(ps, make_consumer(result.utc_offset.emplace(0))),
                done, "+-")
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character) {
    if (fractional_seconds_decimals > 0) {
      auto divisor = 1;
      for (auto i = 0; i < fractional_seconds_decimals; ++i)
        divisor *= 10;
      result.nanosecond = fractional_seconds * (1'000'000'000 / divisor);
    }
    if (result.valid())
      consumer.value(result);
    else
      ps.code = pec::invalid_argument;
  }
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
