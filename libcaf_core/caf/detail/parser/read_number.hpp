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
#include <type_traits>

#include "caf/config.hpp"
#include "caf/detail/consumer.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/read_floating_point.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads the second half of 'n..m' range statement.
///
/// Expect the current position to point at the number *after* the dots:
///
/// ~~~
/// foo = [1..2]
///        ~~~^
/// ~~~
template <class State, class Consumer>
void read_number_range(State& ps, Consumer& consumer, int64_t begin);

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class State, class Consumer, class EnableFloat = std::true_type,
          class EnableRange = std::false_type>
void read_number(State& ps, Consumer& consumer, EnableFloat = {},
                 EnableRange = {}) {
  static constexpr bool enable_float = EnableFloat::value;
  static constexpr bool enable_range = EnableRange::value;
  // Our result when reading an integer number.
  int64_t result = 0;
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(result);
  });
  using odbl = optional<double>;
  // clang-format off
  // Definition of our parser FSM.
  start();
  state(init) {
    transition(init, " \t")
    transition(has_plus, '+')
    transition(has_minus, '-')
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}),
                          done, '.', g.disable())
    epsilon(has_plus)
  }
  // "+" or "-" alone aren't numbers.
  state(has_plus) {
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}),
                          done, '.', g.disable())
    transition(pos_zero, '0')
    epsilon(pos_dec)
  }
  state(has_minus) {
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{0.}, true),
                          done, '.', g.disable())
    transition(neg_zero, '0')
    epsilon(neg_dec)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    transition(start_pos_bin, "bB")
    transition(start_pos_hex, "xX")
    transition_static_if(enable_float || enable_range, pos_dot, '.')
    epsilon(pos_oct)
  }
  term_state(neg_zero) {
    transition(start_neg_bin, "bB")
    transition(start_neg_hex, "xX")
    transition_static_if(enable_float || enable_range, neg_dot, '.')
    epsilon(neg_oct)
  }
  // Binary integers.
  state(start_pos_bin) {
    epsilon(pos_bin)
  }
  term_state(pos_bin) {
    transition(pos_bin, "01", add_ascii<2>(result, ch), pec::integer_overflow)
  }
  state(start_neg_bin) {
    epsilon(neg_bin)
  }
  term_state(neg_bin) {
    transition(neg_bin, "01", sub_ascii<2>(result, ch), pec::integer_underflow)
  }
  // Octal integers.
  state(start_pos_oct) {
    epsilon(pos_oct)
  }
  term_state(pos_oct) {
    transition(pos_oct, octal_chars, add_ascii<8>(result, ch),
               pec::integer_overflow)
  }
  state(start_neg_oct) {
    epsilon(neg_oct)
  }
  term_state(neg_oct) {
    transition(neg_oct, octal_chars, sub_ascii<8>(result, ch),
               pec::integer_underflow)
  }
  // Hexal integers.
  state(start_pos_hex) {
    epsilon(pos_hex)
  }
  term_state(pos_hex) {
    transition(pos_hex, hexadecimal_chars, add_ascii<16>(result, ch),
               pec::integer_overflow)
  }
  state(start_neg_hex) {
    epsilon(neg_hex)
  }
  term_state(neg_hex) {
    transition(neg_hex, hexadecimal_chars, sub_ascii<16>(result, ch),
               pec::integer_underflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(pos_dec) {
    transition(pos_dec, decimal_chars, add_ascii<10>(result, ch),
               pec::integer_overflow)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{result}),
                          done, "eE", g.disable())
    transition_static_if(enable_float || enable_range, pos_dot, '.')
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    transition(neg_dec, decimal_chars, sub_ascii<10>(result, ch),
               pec::integer_underflow)
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{result}, true),
                          done, "eE", g.disable())
    transition_static_if(enable_float || enable_range, neg_dot, '.')
  }
  unstable_state(pos_dot) {
    fsm_transition_static_if(enable_range,
                             read_number_range(ps, consumer, result),
                             done, '.', g.disable())
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{result}),
                          done, any_char, g.disable())
    epsilon(done)
  }
  unstable_state(neg_dot) {
    fsm_transition_static_if(enable_range,
                             read_number_range(ps, consumer, result),
                             done, '.', g.disable())
    fsm_epsilon_static_if(enable_float,
                          read_floating_point(ps, consumer, odbl{result}, true),
                          done, any_char, g.disable())
    epsilon(done)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_number_range(State& ps, Consumer& consumer, int64_t begin) {
  optional<int64_t> end;
  optional<int64_t> step;
  auto end_consumer = make_consumer(end);
  auto step_consumer = make_consumer(step);
  auto generate_2 = [&](int64_t n, int64_t m) {
    if (n <= m)
      while (n <= m)
        consumer.value(n++);
    else
      while (n >= m)
        consumer.value(n--);
  };
  auto generate_3 = [&](int64_t n, int64_t m, int64_t s) {
    if (n == m) {
      consumer.value(n);
      return;
    }
    if (s == 0 || (n > m && s > 0) || (n < m && s < 0)) {
      ps.code = pec::invalid_range_expression;
      return;
    }
    if (n <= m)
      for (auto i = n; i <= m; i += s)
        consumer.value(i);
    else
      for (auto i = n; i >= m; i += s)
        consumer.value(i);
  };
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      if (!end) {
        ps.code = pec::invalid_range_expression;
      } else if (!step) {
        generate_2(begin, *end);
      } else {
        generate_3(begin, *end, *step);
      }
    }
  });
  static constexpr std::false_type no_float = std::false_type{};
  // clang-format off
  // Definition of our parser FSM.
  start();
  state(init) {
    fsm_epsilon(read_number(ps, end_consumer, no_float), after_end_num)
  }
  term_state(after_end_num) {
    transition(first_dot, '.')
  }
  state(first_dot) {
    transition(second_dot, '.')
  }
  state(second_dot) {
    fsm_epsilon(read_number(ps, step_consumer, no_float), done)
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
