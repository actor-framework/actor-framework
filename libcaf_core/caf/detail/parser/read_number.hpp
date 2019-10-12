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

#include "caf/config.hpp"
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

namespace caf {
namespace detail {
namespace parser {

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class State, class Consumer>
void read_number(State& ps, Consumer& consumer) {
  // Our result when reading an integer number.
  int64_t result = 0;
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(result);
  });
  // clang-format off
  // Definition of our parser FSM.
  start();
  state(init) {
    transition(init, " \t")
    transition(has_plus, '+')
    transition(has_minus, '-')
    epsilon(has_plus)
  }
  // "+" or "-" alone aren't numbers.
  state(has_plus) {
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{0.}),
                done, '.', g.disable())
    transition(pos_zero, '0')
    epsilon(pos_dec)
  }
  state(has_minus) {
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{0.}, true),
                done, '.', g.disable())
    transition(neg_zero, '0')
    epsilon(neg_dec)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    transition(start_pos_bin, "bB")
    transition(start_pos_hex, "xX")
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{0.}),
                done, '.', g.disable())
    epsilon(pos_oct)
  }
  term_state(neg_zero) {
    transition(start_neg_bin, "bB")
    transition(start_neg_hex, "xX")
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{0.}, true),
                done, '.', g.disable())
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
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{result}),
                done, "eE", g.disable())
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{result}),
                done, '.', g.disable())
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    transition(neg_dec, decimal_chars, sub_ascii<10>(result, ch),
               pec::integer_underflow)
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{result}, true),
                done, "eE", g.disable())
    fsm_epsilon(read_floating_point(ps, consumer, optional<double>{result}, true),
                done, '.', g.disable())
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
