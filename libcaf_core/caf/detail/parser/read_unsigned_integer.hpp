// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

#include <cstdint>
#include <type_traits>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class State, class Consumer>
void read_unsigned_integer(State& ps, Consumer&& consumer) {
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

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
