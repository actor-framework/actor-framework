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

#include "caf/detail/parser/ec.hpp"
#include "caf/detail/parser/fsm.hpp"
#include "caf/detail/parser/is_char.hpp"
#include "caf/detail/parser/read_atom.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number_or_timespan.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/scope_guard.hpp"

namespace caf {
namespace detail {
namespace parser {

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

template <class Iterator, class Sentinel, class Consumer>
void read_ini_comment(state<Iterator, Sentinel>& ps, Consumer&) {
  start();
  state(init) {
    input(is_char<';'>, await_newline)
  }
  term_state(await_newline) {
    input(is_char<'\n'>, done)
    any_input(await_newline)
  }
  term_state(done) {
    // nop
  }
  fin();
}

template <class Iterator, class Sentinel, class Consumer>
void read_ini_value(state<Iterator, Sentinel>& ps, Consumer& consumer);

template <class Iterator, class Sentinel, class Consumer>
void read_ini_list(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  start();
  state(init) {
    action(is_char<'['>, before_value, consumer.begin_list())
  }
  state(before_value) {
    input(is_char<' '>, before_value)
    input(is_char<'\t'>, before_value)
    input(is_char<'\n'>, before_value)
    action(is_char<']'>, done, consumer.end_list())
    invoke_fsm_if(is_char<';'>, read_ini_comment(ps, consumer), before_value)
    invoke_fsm(read_ini_value(ps, consumer), after_value)
  }
  state(after_value) {
    input(is_char<' '>, after_value)
    input(is_char<'\t'>, after_value)
    input(is_char<'\n'>, after_value)
    input(is_char<','>, before_value)
    action(is_char<']'>, done, consumer.end_list())
    invoke_fsm_if(is_char<';'>, read_ini_comment(ps, consumer), after_value)
  }
  term_state(done) {
    // nop
  }
  fin();
}

template <class Iterator, class Sentinel, class Consumer>
void read_ini_map(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  // TODO: implement me
}

template <class Iterator, class Sentinel, class Consumer>
void read_ini_value(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  auto is_f_or_t = [](char x) {
    return x == 'f' || x == 'F' || x == 't' || x == 'T';
  };
  start();
  state(init) {
    invoke_fsm_if(is_char<'"'>, read_string(ps, consumer), done)
    invoke_fsm_if(is_char<'\''>, read_atom(ps, consumer), done)
    invoke_fsm_if(is_char<'.'>, read_number(ps, consumer), done)
    invoke_fsm_if(is_f_or_t, read_bool(ps, consumer), done)
    invoke_fsm_if(isdigit, read_number_or_timespan(ps, consumer), done)
    invoke_fsm_if(is_char<'['>, read_ini_list(ps, consumer), done)
    invoke_fsm_if(is_char<'{'>, read_ini_map(ps, consumer), done)
  }
  term_state(done) {
    // nop
  }
  fin();
}

/// Reads an INI formatted input.
template <class Iterator, class Sentinel, class Consumer>
void read_ini(state<Iterator, Sentinel>& ps, Consumer& consumer) {
  using std::swap;
  std::string tmp;
  auto is_alnum_or_dash = [](char x) {
    return isalnum(x) || x == '-' || x == '_';
  };
  bool in_section = false;
  auto emit_key = [&] {
    std::string key;
    swap(tmp, key);
    consumer.key(std::move(key));
  };
  auto begin_section = [&] {
    if (in_section)
      consumer.end_map();
    else
      in_section = true;
    emit_key();
    consumer.begin_map();
  };
  auto g = make_scope_guard([&] {
    if (ps.code <= ec::trailing_character && in_section)
      consumer.end_map();
  });
  start();
  // Scanning for first section.
  term_state(init) {
    input(is_char<' '>, init)
    input(is_char<'\t'>, init)
    input(is_char<'\n'>, init)
    invoke_fsm_if(is_char<';'>, read_ini_comment(ps, consumer), init)
    input(is_char<'['>, start_section)
  }
  // Read the section key after reading an '['.
  state(start_section) {
    input(is_char<' '>, start_section)
    input(is_char<'\t'>, start_section)
    action(isalpha, read_section_name, tmp = ch)
  }
  // Reads a section name such as "[foo]".
  state(read_section_name) {
    action(is_alnum_or_dash, read_section_name, tmp += ch)
    epsilon(close_section)
  }
  // Wait for the closing ']', preceded by any number of whitespaces.
  state(close_section) {
    input(is_char<' '>, close_section)
    input(is_char<'\t'>, close_section)
    action(is_char<']'>, dispatch, begin_section())
  }
  // Dispatches to read sections, comments, or key/value pairs.
  term_state(dispatch) {
    input(is_char<' '>, dispatch)
    input(is_char<'\t'>, dispatch)
    input(is_char<'\n'>, dispatch)
    input(is_char<'['>, read_section_name)
    invoke_fsm_if(is_char<';'>, read_ini_comment(ps, consumer), dispatch)
    action(isalnum, read_key_name, tmp = ch)
  }
  // Reads a key of a "key=value" line.
  state(read_key_name) {
    action(is_alnum_or_dash, read_key_name, tmp += ch)
    epsilon(await_assignment)
  }
  // Reads the assignment operator in a "key=value" line.
  state(await_assignment) {
    input(is_char<' '>, await_assignment)
    input(is_char<'\t'>, await_assignment)
    action(is_char<'='>, await_value, emit_key())
  }
  // Reads the value in a "key=value" line.
  state(await_value) {
    input(is_char<' '>, await_value)
    input(is_char<'\t'>, await_value)
    invoke_fsm(read_ini_value(ps, consumer), await_eol)
  }
  // Waits for end-of-line after reading a value
  term_state(await_eol) {
    input(is_char<' '>, await_eol)
    input(is_char<'\t'>, await_eol)
    invoke_fsm_if(is_char<';'>, read_ini_comment(ps, consumer), dispatch)
    input(is_char<'\n'>, dispatch)
  }
  fin();
}

} // namespace parser
} // namespace detail
} // namespace caf
