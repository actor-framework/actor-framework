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
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/add_ascii.hpp"
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
void read_number(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  // Any exponent larger than 511 always overflows.
  static constexpr int max_double_exponent = 511;
  // We assume a simple integer until proven wrong.
  enum result_type_t { integer, positive_double, negative_double };
  auto result_type = integer;
  // Adjusts our mantissa, e.g., 1.23 becomes 123 with a dec_exp of -2.
  auto dec_exp = 0;
  // Exponent part of a floating point literal.
  auto exp = 0;
  // Our result when reading a floating point number.
  auto dbl_res = 0.;
  // Our result when reading an integer number.
  int64_t int_res = 0;
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      if (result_type == integer) {
        consumer.value(int_res);
        return;
      }
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
      static double powerTable[] = {1e1,  1e2,  1e4,   1e8,  1e16,
                                    1e32, 1e64, 1e128, 1e256};
      auto i = 0;
      if (exp < 0) {
        for (auto n = -exp; n != 0; n >>= 1, ++i)
          if (n & 0x01)
            dbl_res /= powerTable[i];
      } else {
        for (auto n = exp; n != 0; n >>= 1, ++i)
          if (n & 0x01)
            dbl_res *= powerTable[i];
      }
      // 4) Fix sign and call consumer.
      consumer.value(result_type == positive_double ? dbl_res : -dbl_res);
    }
  });
  // Switches from parsing an integer to parsing a double.
  auto ch_res = [&](result_type_t x) {
    CAF_ASSERT(result_type == integer);
    result_type = x;
    // We parse doubles as positive number and restore the sign in `g`.
    dbl_res = result_type == negative_double
              ? -static_cast<double>(int_res)
              : static_cast<double>(int_res);
  };
  // Reads the a decimal place.
  auto rd_decimal = [&](char c) {
    --dec_exp;
    return add_ascii<10>(dbl_res, c);
  };
  // Definition of our parser FSM.
  start();
  state(init) {
    transition(init, " \t")
    transition(has_plus, '+')
    transition(has_minus, '-')
    transition(pos_zero, '0')
    epsilon(has_plus)
  }
  // "+" or "-" alone aren't numbers.
  state(has_plus) {
    transition(leading_dot, '.', ch_res(positive_double))
    transition(pos_zero, '0')
    epsilon(pos_dec)
  }
  state(has_minus) {
    transition(leading_dot, '.', ch_res(negative_double))
    transition(neg_zero, '0')
    epsilon(neg_dec)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    transition(start_pos_bin, "bB")
    transition(start_pos_hex, "xX")
    transition(trailing_dot, '.', ch_res(positive_double))
    epsilon(pos_oct)
  }
  term_state(neg_zero) {
    transition(start_neg_bin, "bB")
    transition(start_neg_hex, "xX")
    transition(trailing_dot, '.', ch_res(negative_double))
    epsilon(neg_oct)
  }
  // Binary integers.
  state(start_pos_bin) {
    epsilon(pos_bin)
  }
  term_state(pos_bin) {
    transition(pos_bin, "01", add_ascii<2>(int_res, ch), pec::integer_overflow)
  }
  state(start_neg_bin) {
    epsilon(neg_bin)
  }
  term_state(neg_bin) {
    transition(neg_bin, "01", sub_ascii<2>(int_res, ch), pec::integer_underflow)
  }
  // Octal integers.
  state(start_pos_oct) {
    epsilon(pos_oct)
  }
  term_state(pos_oct) {
    transition(pos_oct, octal_chars, add_ascii<8>(int_res, ch),
               pec::integer_overflow)
  }
  state(start_neg_oct) {
    epsilon(neg_oct)
  }
  term_state(neg_oct) {
    transition(neg_oct, octal_chars, sub_ascii<8>(int_res, ch),
               pec::integer_underflow)
  }
  // Hexal integers.
  state(start_pos_hex) {
    epsilon(pos_hex)
  }
  term_state(pos_hex) {
    transition(pos_hex, hexadecimal_chars, add_ascii<16>(int_res, ch),
               pec::integer_overflow)
  }
  state(start_neg_hex) {
    epsilon(neg_hex)
  }
  term_state(neg_hex) {
    transition(neg_hex, hexadecimal_chars, sub_ascii<16>(int_res, ch),
               pec::integer_underflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(pos_dec) {
    transition(pos_dec, decimal_chars, add_ascii<10>(int_res, ch),
               pec::integer_overflow)
    transition(has_e, "eE", ch_res(positive_double))
    transition(trailing_dot, '.', ch_res(positive_double))
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    transition(neg_dec, decimal_chars, sub_ascii<10>(int_res, ch),
               pec::integer_underflow)
    transition(has_e, "eE", ch_res(negative_double))
    transition(trailing_dot, '.', ch_res(negative_double))
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
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
