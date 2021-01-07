// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include <cstdint>
#include <string>

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

/// Reads a quoted or unquoted string. Quoted strings allow escaping, while
/// unquoted strings may only include alphanumeric characters.
template <class State, class Consumer>
void read_string(State& ps, Consumer&& consumer) {
  // Allow Consumer to be a string&, in which case we simply store the result
  // directly in the reference.
  using res_type
    = std::conditional_t<std::is_same<Consumer, std::string&>::value,
                         std::string&, std::string>;
  auto init_res = [](auto& c) -> res_type {
    if constexpr (std::is_same<res_type, std::string&>::value) {
      c.clear();
      return c;
    } else {
      return std::string{};
    }
  };
  res_type res = init_res(consumer);
  auto g = caf::detail::make_scope_guard([&] {
    if constexpr (std::is_same<res_type, std::string>::value)
      if (ps.code <= pec::trailing_character)
        consumer.value(std::move(res));
  });
  static constexpr char single_quote = '\'';
  static constexpr char double_quote = '"';
  char quote_mark = '\0';
  // clang-format off
  start();
  state(init) {
    transition(init, " \t")
    transition(read_chars, double_quote, quote_mark = double_quote)
    transition(read_chars, single_quote, quote_mark = single_quote)
    transition(read_unquoted_chars, alphanumeric_chars, res += ch)
  }
  state(read_chars) {
    transition(escape, '\\')
    transition_if(quote_mark == double_quote, done, double_quote)
    transition_if(quote_mark == single_quote, done, single_quote)
    error_transition(pec::unexpected_newline, '\n')
    transition(read_chars, any_char, res += ch)
  }
  state(escape) {
    transition(read_chars, 'n', res += '\n')
    transition(read_chars, 'r', res += '\r')
    transition(read_chars, 't', res += '\t')
    transition(read_chars, '\\', res += '\\')
    transition_if(quote_mark == double_quote, read_chars, double_quote,
                  res += double_quote)
    transition_if(quote_mark == single_quote, read_chars, single_quote,
                  res += single_quote)
    error_transition(pec::invalid_escape_sequence)
  }
  term_state(read_unquoted_chars) {
    transition(read_unquoted_chars, alphanumeric_chars, res += ch)
    epsilon(done)
  }
  term_state(done) {
    transition(done, " \t")
  }
  fin();
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
