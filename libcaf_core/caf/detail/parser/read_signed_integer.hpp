/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
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
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/state.hpp"
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
template <class Iterator, class Sentinel, class Consumer>
void read_signed_integer(state<Iterator, Sentinel>& ps, Consumer&& consumer) {
  using consumer_type = typename std::decay<Consumer>::type;
  using value_type = typename consumer_type::value_type;
  static_assert(std::is_integral<value_type>::value
                  && std::is_signed<value_type>::value,
                "expected a signed integer type");
  value_type result = 0;
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      consumer.value(std::move(result));
    }
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
    transition(pos_zero, '0')
    epsilon(pos_dec, decimal_chars)
  }
  state(has_minus) {
    transition(neg_zero, '0')
    epsilon(neg_dec, decimal_chars)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    transition(start_pos_bin, "bB")
    transition(start_pos_hex, "xX")
    epsilon(pos_oct)
  }
  term_state(neg_zero) {
    transition(start_neg_bin, "bB")
    transition(start_neg_hex, "xX")
    epsilon(neg_oct)
  }
  // Binary integers.
  state(start_pos_bin) {
    epsilon(pos_bin, "01")
  }
  term_state(pos_bin) {
    transition(pos_bin, "01", add_ascii<2>(result, ch), pec::integer_overflow)
  }
  state(start_neg_bin) {
    epsilon(neg_bin, "01")
  }
  term_state(neg_bin) {
    transition(neg_bin, "01", sub_ascii<2>(result, ch), pec::integer_underflow)
  }
  // Octal integers.
  state(start_pos_oct) {
    epsilon(pos_oct, octal_chars)
  }
  term_state(pos_oct) {
    transition(pos_oct, octal_chars, add_ascii<8>(result, ch),
               pec::integer_overflow)
  }
  state(start_neg_oct) {
    epsilon(neg_oct, octal_chars)
  }
  term_state(neg_oct) {
    transition(neg_oct, octal_chars, sub_ascii<8>(result, ch),
               pec::integer_underflow)
  }
  // Hexal integers.
  state(start_pos_hex) {
    epsilon(pos_hex, hexadecimal_chars)
  }
  term_state(pos_hex) {
    transition(pos_hex, hexadecimal_chars, add_ascii<16>(result, ch),
               pec::integer_overflow)
  }
  state(start_neg_hex) {
    epsilon(neg_hex, hexadecimal_chars)
  }
  term_state(neg_hex) {
    transition(neg_hex, hexadecimal_chars, sub_ascii<16>(result, ch),
               pec::integer_underflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(pos_dec) {
    transition(pos_dec, decimal_chars, add_ascii<10>(result, ch),
               pec::integer_overflow)
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    transition(neg_dec, decimal_chars, sub_ascii<10>(result, ch),
               pec::integer_underflow)
  }
  fin();
  // clang-format on
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
