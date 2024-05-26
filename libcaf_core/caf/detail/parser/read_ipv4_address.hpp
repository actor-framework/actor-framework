// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/is_digit.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/pec.hpp"

#include <cstdint>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

struct read_ipv4_octet_consumer {
  std::array<uint8_t, 4> bytes;
  size_t octets = 0;

  void value(uint8_t octet) {
    bytes[octets++] = octet;
  }
};

template <class State, class Consumer>
void read_ipv4_octet(State& ps, Consumer& consumer) {
  uint8_t res = 0;
  // Reads the a decimal place.
  auto rd_decimal = [&](char c) { return add_ascii<10>(res, c); };
  // clang-format off
  start();
  state(init) {
    transition(read, decimal_chars, rd_decimal(ch), pec::integer_overflow)
  }
  term_state(read) {
    transition(read, decimal_chars, rd_decimal(ch), pec::integer_overflow)
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character)
    consumer.value(res);
}

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class State, class Consumer>
void read_ipv4_address(State& ps, Consumer&& consumer) {
  read_ipv4_octet_consumer f;
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_ipv4_octet(ps, f), rd_dot, decimal_chars)
  }
  state(rd_dot) {
    transition(rd_oct, '.')
  }
  state(rd_oct) {
    fsm_epsilon_if(f.octets < 3, read_ipv4_octet(ps, f), rd_dot, decimal_chars)
    fsm_epsilon_if(f.octets == 3, read_ipv4_octet(ps, f), done, decimal_chars)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
  if (ps.code <= pec::trailing_character) {
    ipv4_address result{f.bytes};
    consumer.value(std::move(result));
  }
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
