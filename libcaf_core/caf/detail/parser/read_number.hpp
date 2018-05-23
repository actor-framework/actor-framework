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

#include "caf/detail/scope_guard.hpp"

#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/ec.hpp"
#include "caf/detail/parser/fsm.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/detail/parser/sub_ascii.hpp"

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
    if (ps.code <= ec::trailing_character) {
      if (result_type == integer) {
        consumer.value(int_res);
        return;
      }
      // Compute final floating point number.
      // 1) Fix the exponent.
      exp += dec_exp;
      // 2) Check whether exponent is in valid range.
      if (exp < -max_double_exponent) {
        ps.code = ec::exponent_underflow;
        return;
      }
      if (exp > max_double_exponent) {
        ps.code = ec::exponent_overflow;
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
    input(is_char<' '>, init)
    input(is_char<'\t'>, init)
    input(is_char<'+'>, has_plus)
    input(is_char<'-'>, has_minus)
    input(is_char<'0'>, pos_zero)
    epsilon(has_plus)
  }
  // "+" or "-" alone aren't numbers.
  state(has_plus) {
    action(is_char<'.'>, leading_dot, ch_res(positive_double))
    input(is_char<'0'>, pos_zero)
    epsilon(pos_dec)
  }
  state(has_minus) {
    action(is_char<'.'>, leading_dot, ch_res(negative_double))
    input(is_char<'0'>, neg_zero)
    epsilon(neg_dec)
  }
  // Disambiguate base.
  term_state(pos_zero) {
    input(is_ichar<'b'>, start_pos_bin)
    input(is_ichar<'x'>, start_pos_hex)
    action(is_char<'.'>, trailing_dot, ch_res(positive_double))
    epsilon(pos_oct)
  }
  term_state(neg_zero) {
    input(is_ichar<'b'>, start_neg_bin)
    input(is_ichar<'x'>, start_neg_hex)
    action(is_char<'.'>, trailing_dot, ch_res(negative_double))
    epsilon(neg_oct)
  }
  // Binary integers.
  state(start_pos_bin) {
    epsilon(pos_bin)
  }
  term_state(pos_bin) {
    checked_action(is_digit<2>, pos_bin, add_ascii<2>(int_res, ch),
                   ec::integer_overflow)
  }
  state(start_neg_bin) {
    epsilon(neg_bin)
  }
  term_state(neg_bin) {
    checked_action(is_digit<2>, neg_bin, sub_ascii<2>(int_res, ch),
                   ec::integer_underflow)
  }
  // Octal integers.
  state(start_pos_oct) {
    epsilon(pos_oct)
  }
  term_state(pos_oct) {
    checked_action(is_digit<8>, pos_oct, add_ascii<8>(int_res, ch),
                   ec::integer_overflow)
  }
  state(start_neg_oct) {
    epsilon(neg_oct)
  }
  term_state(neg_oct) {
    checked_action(is_digit<8>, neg_oct, sub_ascii<8>(int_res, ch),
                   ec::integer_underflow)
  }
  // Hexal integers.
  state(start_pos_hex) {
    epsilon(pos_hex)
  }
  term_state(pos_hex) {
    checked_action(is_digit<16>, pos_hex, add_ascii<16>(int_res, ch),
                   ec::integer_overflow)
  }
  state(start_neg_hex) {
    epsilon(neg_hex)
  }
  term_state(neg_hex) {
    checked_action(is_digit<16>, neg_hex, sub_ascii<16>(int_res, ch),
                   ec::integer_underflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(pos_dec) {
    checked_action(is_digit<10>, pos_dec, add_ascii<10>(int_res, ch),
                   ec::integer_overflow)
    action(is_ichar<'e'>, has_e, ch_res(positive_double))
    action(is_char<'.'>, trailing_dot, ch_res(positive_double))
  }
  // Reads the integer part of the mantissa or a negative decimal integer.
  term_state(neg_dec) {
    checked_action(is_digit<10>, neg_dec, sub_ascii<10>(int_res, ch),
                   ec::integer_underflow)
    action(is_ichar<'e'>, has_e, ch_res(negative_double))
    action(is_char<'.'>, trailing_dot, ch_res(negative_double))
  }
  // ".", "+.", etc. aren't valid numbers, so this state isn't terminal.
  state(leading_dot) {
    checked_action(is_digit<10>, after_dot, rd_decimal(ch),
                   ec::exponent_underflow)
  }
  // "1." is a valid number, so a trailing dot is a terminal state.
  term_state(trailing_dot) {
    checked_action(is_digit<10>, after_dot, rd_decimal(ch),
                   ec::exponent_underflow)
    input(is_ichar<'e'>, has_e)
  }
  // Read the decimal part of a mantissa.
  term_state(after_dot) {
    checked_action(is_digit<10>, after_dot, rd_decimal(ch),
                   ec::exponent_underflow)
    input(is_ichar<'e'>, has_e)
  }
  // "...e", "...e+", and "...e-" aren't valid numbers, so these states are not
  // terminal.
  state(has_e) {
    input(is_char<'+'>, has_plus_after_e)
    input(is_char<'-'>, has_minus_after_e)
    checked_action(is_digit<10>, pos_exp, add_ascii<10>(exp, ch),
                   ec::exponent_overflow)
  }
  state(has_plus_after_e) {
    checked_action(is_digit<10>, pos_exp, add_ascii<10>(exp, ch),
                   ec::exponent_overflow)
  }
  state(has_minus_after_e) {
    checked_action(is_digit<10>, neg_exp, sub_ascii<10>(exp, ch),
                   ec::exponent_underflow)
  }
  // Read a positive exponent.
  term_state(pos_exp) {
    checked_action(is_digit<10>, pos_exp, add_ascii<10>(exp, ch),
                   ec::exponent_overflow)
  }
  // Read a negative exponent.
  term_state(neg_exp) {
    checked_action(is_digit<10>, neg_exp, sub_ascii<10>(exp, ch),
                   ec::exponent_underflow)
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

