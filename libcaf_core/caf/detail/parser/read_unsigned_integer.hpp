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
void read_unsigned_integer(state<Iterator, Sentinel>& ps, Consumer&& consumer) {
  using consumer_type = typename std::decay<Consumer>::type;
  using value_type = typename consumer_type::value_type;
  static_assert(std::is_integral<value_type>::value
                  && std::is_unsigned<value_type>::value,
                "expected an unsigned integer type");
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
    epsilon(has_plus)
  }
  // "+" or "-" alone aren't numbers.
  state(has_plus) {
    transition(zero, '0')
    epsilon(dec, decimal_chars)
  }
  // Disambiguate base.
  term_state(zero) {
    transition(start_bin, "bB")
    transition(start_hex, "xX")
    epsilon(oct)
  }
  // Binary integers.
  state(start_bin) {
    epsilon(bin, "01")
  }
  term_state(bin) {
    transition(bin, "01", add_ascii<2>(result, ch), pec::integer_overflow)
  }
  // Octal integers.
  state(start_oct) {
    epsilon(oct, octal_chars)
  }
  term_state(oct) {
    transition(oct, octal_chars, add_ascii<8>(result, ch),
               pec::integer_overflow)
  }
  // Hexal integers.
  state(start_hex) {
    epsilon(hex, hexadecimal_chars)
  }
  term_state(hex) {
    transition(hex, hexadecimal_chars, add_ascii<16>(result, ch),
               pec::integer_overflow)
  }
  // Reads the integer part of the mantissa or a positive decimal integer.
  term_state(dec) {
    transition(dec, decimal_chars, add_ascii<10>(result, ch),
               pec::integer_overflow)
  }
  fin();
  // clang-format on
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
