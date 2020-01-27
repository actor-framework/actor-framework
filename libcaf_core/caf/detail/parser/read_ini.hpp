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

#include <ctype.h>

#include <stack>

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number_or_timespan.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/uri_builder.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

// Example input:
//
// [section1]
// value1 = 123
// value2 = "string"
// subsection1 = {
// value3 = 1.23
// value4 = 4e20
// }
// [section2]
// value5 = 'atom'
// value6 = [1, 'two', "three", {
//   a = "b",
//   b = "c",
// }]
//

template <class State, class Consumer>
void read_ini_comment(State& ps, Consumer&&) {
  // clang-format off
  start();
  term_state(init) {
    transition(done, '\n')
    transition(init)
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer, class InsideList = std::false_type>
void read_ini_value(State& ps, Consumer&& consumer,
                    InsideList inside_list = {});

template <class State, class Consumer>
void read_ini_list(State& ps, Consumer&& consumer) {
  // clang-format off
  start();
  state(init) {
    epsilon(before_value)
  }
  state(before_value) {
    transition(before_value, " \t\n")
    transition(done, ']', consumer.end_list())
    fsm_epsilon(read_ini_comment(ps, consumer), before_value, ';')
    fsm_epsilon(read_ini_value(ps, consumer, std::true_type{}), after_value)
  }
  state(after_value) {
    transition(after_value, " \t\n")
    transition(before_value, ',')
    transition(done, ']', consumer.end_list())
    fsm_epsilon(read_ini_comment(ps, consumer), after_value, ';')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_ini_map(State& ps, Consumer&& consumer) {
  std::string key;
  auto alnum_or_dash
    = [](char x) { return isalnum(x) || x == '-' || x == '_'; };
  // clang-format off
  start();
  state(init) {
    epsilon(await_key_name)
  }
  state(await_key_name) {
    transition(await_key_name, " \t\n")
    fsm_epsilon(read_ini_comment(ps, consumer), await_key_name, ';')
    transition(read_key_name, alphanumeric_chars, key = ch)
    transition(done, '}', consumer.end_map())
  }
  // Reads a key of a "key=value" line.
  state(read_key_name) {
    transition(read_key_name, alnum_or_dash, key += ch)
    epsilon(await_assignment)
  }
  // Reads the assignment operator in a "key=value" line.
  state(await_assignment) {
    transition(await_assignment, " \t")
    transition(await_value, '=', consumer.key(std::move(key)))
    epsilon(await_value, '{', consumer.key(std::move(key)))
  }
  // Reads the value in a "key=value" line.
  state(await_value) {
    transition(await_value, " \t")
    fsm_epsilon(read_ini_value(ps, consumer), after_value)
  }
  // Waits for end-of-line after reading a value
  state(after_value) {
    transition(after_value, " \t")
    transition(had_newline, "\n")
    transition(await_key_name, ',')
    transition(done, '}', consumer.end_map())
    fsm_epsilon(read_ini_comment(ps, consumer), had_newline, ';')
  }
  // Allows users to skip the ',' for separating key/value pairs
  state(had_newline) {
    transition(had_newline, " \t\n")
    transition(await_key_name, ',')
    transition(done, '}', consumer.end_map())
    fsm_epsilon(read_ini_comment(ps, consumer), had_newline, ';')
    epsilon(read_key_name, alnum_or_dash)
  }
  term_state(done) {
    //nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_ini_uri(State& ps, Consumer&& consumer) {
  uri_builder builder;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.value(builder.make());
  });
  // clang-format off
  start();
  state(init) {
    transition(init, " \t\n")
    transition(before_uri, '<')
  }
  state(before_uri) {
    transition(before_uri, " \t\n")
    fsm_epsilon(read_uri(ps, builder), after_uri)
  }
  state(after_uri) {
    transition(after_uri, " \t\n")
    transition(done, '>')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer, class InsideList>
void read_ini_value(State& ps, Consumer&& consumer, InsideList inside_list) {
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_string(ps, consumer), done, '"')
    fsm_epsilon(read_number(ps, consumer), done, '.')
    fsm_epsilon(read_bool(ps, consumer), done, "ft")
    fsm_epsilon(read_number_or_timespan(ps, consumer, inside_list),
                done, "0123456789+-")
    fsm_epsilon(read_ini_uri(ps, consumer), done, '<')
    fsm_transition(read_ini_list(ps, consumer.begin_list()), done, '[')
    fsm_transition(read_ini_map(ps, consumer.begin_map()), done, '{')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

/// Reads an INI formatted input.
template <class State, class Consumer>
void read_ini_section(State& ps, Consumer&& consumer) {
  using std::swap;
  std::string tmp;
  auto alnum = [](char x) { return isalnum(x) || x == '_'; };
  auto alnum_or_dash
    = [](char x) { return isalnum(x) || x == '_' || x == '-'; };
  auto emit_key = [&] {
    std::string key;
    swap(tmp, key);
    consumer.key(std::move(key));
  };
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.end_map();
  });
  // clang-format off
  start();
  // Dispatches to read sections, comments, or key/value pairs.
  term_state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_ini_comment(ps, consumer), init, ';')
    transition(read_key_name, alnum, tmp = ch)
  }
  // Reads a key of a "key=value" line.
  state(read_key_name) {
    transition(read_key_name, alnum_or_dash, tmp += ch)
    epsilon(await_assignment)
  }
  // Reads the assignment operator in a "key=value" line.
  state(await_assignment) {
    transition(await_assignment, " \t")
    transition(await_value, '=', emit_key())
    // The '=' is optional for maps, i.e., `key = {}` == `key {}`.
    epsilon(await_value, '{', emit_key())
  }
  // Reads the value in a "key=value" line.
  state(await_value) {
    transition(await_value, " \t")
    fsm_epsilon(read_ini_value(ps, consumer), await_eol)
  }
  // Waits for end-of-line after reading a value
  term_state(await_eol) {
    transition(await_eol, " \t")
    fsm_epsilon(read_ini_comment(ps, consumer), init, ';')
    transition(init, '\n')
  }
  fin();
  // clang-format on
}

/// Reads a nested group, e.g., "[foo.bar]" would consume "[foo.]" in read_ini
/// and then delegate to this function for parsing "bar]".
template <class State, class Consumer>
void read_nested_group(State& ps, Consumer&& consumer) {
  using std::swap;
  std::string key;
  auto alnum = [](char x) { return isalnum(x) || x == '_'; };
  auto alnum_or_dash
    = [](char x) { return isalnum(x) || x == '_' || x == '-'; };
  auto begin_section = [&]() -> decltype(consumer.begin_map()) {
    consumer.key(std::move(key));
    return consumer.begin_map();
  };
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.end_map();
  });
  // clang-format off
  start();
  // Scanning for first section.
  state(init) {
    epsilon(read_sub_section, alnum)
  }
  // Read the sub section key after reading an '[xxx.'.
  state(read_sub_section) {
    transition(read_sub_section, alnum_or_dash, key += ch)
    fsm_transition(read_nested_group(ps, begin_section()), done, '.')
    fsm_transition(read_ini_section(ps, begin_section()), done, ']')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

/// Reads an INI formatted input.
template <class State, class Consumer>
void read_ini(State& ps, Consumer&& consumer) {
  using std::swap;
  std::string tmp{"global"};
  auto alnum = [](char x) { return isalnum(x) || x == '_'; };
  auto alnum_or_dash
    = [](char x) { return isalnum(x) || x == '_' || x == '-'; };
  auto begin_section = [&]() -> decltype(consumer.begin_map()) {
    std::string key;
    swap(tmp, key);
    consumer.key(std::move(key));
    return consumer.begin_map();
  };
  // clang-format off
  start();
  // Scanning for first section.
  term_state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_ini_comment(ps, consumer), init, ';')
    transition(start_section, '[')
    fsm_epsilon_if(tmp == "global", read_ini_section(ps, begin_section()),
                   return_to_global, alnum)
  }
  // Read the section key after reading an '['.
  state(start_section) {
    transition(start_section, " \t")
    transition(read_section_name, alnum, tmp = ch)
  }
  // Reads a section name such as "[foo]".
  state(read_section_name) {
    transition(read_section_name, alnum_or_dash, tmp += ch)
    fsm_transition(read_nested_group(ps, begin_section()), return_to_global, '.')
    epsilon(close_section)
  }
  // Wait for the closing ']', preceded by any number of whitespaces.
  state(close_section) {
    transition(close_section, " \t")
    fsm_transition(read_ini_section(ps, begin_section()), return_to_global, ']')
  }
  unstable_state(return_to_global) {
    epsilon(init, any_char, tmp = "global")
  }
  fin();
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
