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
#include "caf/detail/parser/state.hpp"
#include "caf/detail/parser/sub_ascii.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf {
namespace detail {
namespace parser {

struct read_ipv4_octet_consumer {
  std::array<uint8_t, 4> bytes;
  int octets = 0;

  inline void value(uint8_t octet) {
    bytes[octets++] = octet;
  }
};

template <class Iterator, class Sentinel, class Consumer>
void read_ipv4_octet(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  uint8_t res = 0;
  // Reads the a decimal place.
  auto rd_decimal = [&](char c) {
    return add_ascii<10>(res, c);
  };
  // Computes the result on success.
  auto g = caf::detail::make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(res);
  });
  start();
  state(init) {
    transition(read, decimal_chars, rd_decimal(ch), pec::integer_overflow)
  }
  term_state(read) {
    transition(read, decimal_chars, rd_decimal(ch), pec::integer_overflow)
  }
  fin();
}

/// Reads a number, i.e., on success produces either an `int64_t` or a
/// `double`.
template <class Iterator, class Sentinel, class Consumer>
void read_ipv4_address(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  read_ipv4_octet_consumer f;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character) {
      ipv4_address result{f.bytes};
      consumer.value(result);
    }
  });
  start();
  state(init) {
    fsm_epsilon(read_ipv4_octet(ps, f), rd_dot, decimal_chars)
  }
  state(rd_dot) {
    transition(rd_oct, '.')
  }
  state(rd_oct) {
    fsm_epsilon_if(f.octets < 3, read_ipv4_octet(ps, f), rd_dot, decimal_chars)
    fsm_epsilon_if(f.octets == 3, read_ipv4_octet(ps, f), done)
  }
  term_state(done) {
    // nop
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
