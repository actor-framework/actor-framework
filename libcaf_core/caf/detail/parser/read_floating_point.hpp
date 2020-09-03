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
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/optional.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a floating point number (`float` or `double`).
/// @param ps The parser state.
/// @param consumer Sink for generated values.
/// @param start_value Allows another parser to pre-initialize this parser with
///                    the pre-decimal value.
template <class State, class Consumer, class ValueType>
void read_floating_point(State& ps, Consumer&& consumer,
                         optional<ValueType> start_value,
                         bool negative = false) {
  // Any exponent larger than 511 always overflows.
  static constexpr int max_double_exponent = 511;
  // We assume a simple integer until proven wrong.
  enum sign_t { plus, minus };
  sign_t sign;
  ValueType result;
  if (start_value == none) {
    sign = plus;
    result = 0;
  } else if (*start_value < 0) {
    sign = minus;
    result = -*start_value;
  } else if (negative) {
    sign = minus;
    result = *start_value;
  } else {
    sign = plus;
    result = *start_value;
  }
  // Adjusts our mantissa, e.g., 1.23 becomes 123 with a dec_exp of -2.
  auto dec_exp = 0;
  // Exponent part of a floating point literal.
  auto exp = 0;
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      // Compute final floating point number.
      // 1) Fix the exponent.
      exp += dec_exp;
      // 2) Check whether exponent is in valid range.
      if (exp < -max_double_exponent) {
        ps.code = pec::exponent_underflow;
        return;
      }
      if (exp > max_double_exponent) {
        ps.code = pec::exponent_overflow;
        return;
      }
      // 3) Scale result.
      // Pre-computed powers of 10 for the scaling loop.
      static double powerTable[]
        = {1e1, 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256};
      auto i = 0;
      if (exp < 0) {
        for (auto n = -exp; n != 0; n >>= 1, ++i)
          if (n & 0x01)
            result /= powerTable[i];
      } else {
        for (auto n = exp; n != 0; n >>= 1, ++i)
          if (n & 0x01)
            result *= powerTable[i];
      }
      // 4) Fix sign and call consumer.
      consumer.value(sign == plus ? result : -result);
    }
  });
  // Reads the a decimal place.
  auto rd_decimal = [&](char c) {
    --dec_exp;
    return add_ascii<10>(result, c);
  };
  // clang-format off
  // Definition of our parser FSM.
  start();
  unstable_state(init) {
    epsilon_if(start_value == none, regular_init)
    epsilon(after_dec, "eE.")
    epsilon(after_dot, any_char)
  }
  state(regular_init) {
    transition(regular_init, " \t")
    transition(has_sign, '+')
    transition(has_sign, '-', sign = minus)
    epsilon(has_sign)
  }
  // "+" or "-" alone aren't numbers.
  state(has_sign) {
    transition(leading_dot, '.')
    transition(zero, '0')
    epsilon(dec, decimal_chars)
  }
  term_state(zero) {
    transition(trailing_dot, '.')
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(dec) {
    transition(dec, decimal_chars, add_ascii<10>(result, ch),
               pec::integer_overflow)
    epsilon(after_dec, "eE.")
  }
  state(after_dec) {
    transition(has_e, "eE")
    transition(trailing_dot, '.')
  }
  // ".", "+.", etc. aren't valid numbers, so this state isn't terminal.
  state(leading_dot) {
    transition(after_dot, decimal_chars, rd_decimal(ch), pec::exponent_underflow)
  }
  // "1." is a valid number, so a trailing dot is a terminal state.
  term_state(trailing_dot) {
    epsilon(after_dot)
  }
  // Read the decimal part of a mantissa.
  term_state(after_dot) {
    transition(after_dot, decimal_chars, rd_decimal(ch), pec::exponent_underflow)
    transition(has_e, "eE")
  }
  // "...e", "...e+", and "...e-" aren't valid numbers, so these states are not
  // terminal.
  state(has_e) {
    transition(has_plus_after_e, '+')
    transition(has_minus_after_e, '-')
    transition(pos_exp, decimal_chars, add_ascii<10>(exp, ch),
               pec::exponent_overflow)
  }
  state(has_plus_after_e) {
    transition(pos_exp, decimal_chars, add_ascii<10>(exp, ch),
               pec::exponent_overflow)
  }
  state(has_minus_after_e) {
    transition(neg_exp, decimal_chars, sub_ascii<10>(exp, ch),
               pec::exponent_underflow)
  }
  // Read a positive exponent.
  term_state(pos_exp) {
    transition(pos_exp, decimal_chars, add_ascii<10>(exp, ch),
               pec::exponent_overflow)
  }
  // Read a negative exponent.
  term_state(neg_exp) {
    transition(neg_exp, decimal_chars, sub_ascii<10>(exp, ch),
               pec::exponent_underflow)
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_floating_point(State& ps, Consumer&& consumer) {
  using consumer_type = typename std::decay<Consumer>::type;
  using value_type = typename consumer_type::value_type;
  return read_floating_point(ps, consumer, optional<value_type>{});
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
