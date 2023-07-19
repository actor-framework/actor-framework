// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_number_or_timespan.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/uri_builder.hpp"

#include <ctype.h>

#include <stack>

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

// Example input:
//
// section1 {
//   value1 = 123
//   value2 = "string"
//   subsection1 = {
//     value3 = 1.23
//     value4 = 4e20
//   }
// }
// section2 {
//   value5 = 'atom'
//   value6 = [1, 'two', "three", {
//     a = "b",
//     b = "c",
//   }]
// }
//

template <class State, class Consumer>
void read_config_comment(State& ps, Consumer&&) {
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
void read_config_value(State& ps, Consumer&& consumer,
                       InsideList inside_list = {});

template <class State, class Consumer>
void read_config_list(State& ps, Consumer&& consumer) {
  // clang-format off
  start();
  state(init) {
    epsilon(before_value)
  }
  state(before_value) {
    transition(before_value, " \t\n")
    transition(done, ']', consumer.end_list())
    fsm_epsilon(read_config_comment(ps, consumer), before_value, '#')
    fsm_epsilon(read_config_value(ps, consumer, std::true_type{}), after_value)
  }
  state(after_value) {
    transition(after_value, " \t\n")
    transition(before_value, ',')
    transition(done, ']', consumer.end_list())
    fsm_epsilon(read_config_comment(ps, consumer), after_value, '#')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

// Like read_config_list, but without surrounding '[]'.
template <class State, class Consumer>
void lift_config_list(State& ps, Consumer&& consumer) {
  // clang-format off
  start();
  state(init) {
    epsilon(before_value)
  }
  term_state(before_value) {
    transition(before_value, " \t\n")
    fsm_epsilon(read_config_comment(ps, consumer), before_value, '#')
    fsm_epsilon(read_config_value(ps, consumer, std::true_type{}), after_value)
  }
  term_state(after_value) {
    transition(after_value, " \t\n")
    transition(before_value, ',')
    fsm_epsilon(read_config_comment(ps, consumer), after_value, '#')
  }
  fin();
  // clang-format on
}

template <bool Nested = true, class State, class Consumer>
void read_config_map(State& ps, Consumer&& consumer) {
  std::string key;
  auto alnum_or_dash = [](char x) {
    return isalnum(x) || x == '-' || x == '_';
  };
  auto set_key = [&consumer, &key] {
    std::string tmp;
    tmp.swap(key);
    consumer.key(std::move(tmp));
  };
  auto recurse = [&consumer, &set_key]() -> decltype(auto) {
    set_key();
    return consumer.begin_map();
  };
  // clang-format off
  start();
  term_state(init) {
    epsilon(await_key_name)
  }
  state(await_key_name) {
    transition(await_key_name, " \t\n")
    fsm_epsilon(read_config_comment(ps, consumer), await_key_name, '#')
    fsm_epsilon(read_string(ps, key), await_assignment, quote_marks)
    transition(read_key_name, alnum_or_dash, key = ch)
    transition_if(Nested, done, '}', consumer.end_map())
  }
  // Reads a key of a "key=value" line.
  state(read_key_name) {
    transition(read_key_name, alnum_or_dash, key += ch)
    fsm_transition(read_config_map(ps, recurse()), done, '.')
    epsilon(await_assignment)
  }
  // Reads the assignment operator in a "key=value" line.
  state(await_assignment) {
    transition(await_assignment, " \t")
    transition(await_value, "=:", set_key())
    epsilon(await_value, '{', set_key())
  }
  // Reads the value in a "key=value" line.
  state(await_value) {
    transition(await_value, " \t")
    fsm_epsilon(read_config_value(ps, consumer), after_value)
  }
  // Waits for end-of-line after reading a value
  unstable_state(after_value) {
    transition(after_value, " \t")
    transition(had_newline, "\n")
    transition_if(!Nested, after_comma, ',')
    transition(await_key_name, ',')
    transition_if(Nested, done, '}', consumer.end_map())
    fsm_epsilon(read_config_comment(ps, consumer), had_newline, '#')
    epsilon_if(!Nested, done)
    epsilon(unexpected_end_of_input)
  }
  // Allows users to skip the ',' for separating key/value pairs
  unstable_state(had_newline) {
    transition(had_newline, " \t\n")
    transition(await_key_name, ',')
    transition_if(Nested, done, '}', consumer.end_map())
    fsm_epsilon(read_config_comment(ps, consumer), had_newline, '#')
    fsm_epsilon(read_string(ps, key), await_assignment, quote_marks)
    epsilon(read_key_name, alnum_or_dash)
    epsilon_if(!Nested, done)
    epsilon(unexpected_end_of_input)
  }
  term_state(after_comma) {
    epsilon(await_key_name)
  }
  state(unexpected_end_of_input) {
    // no transitions, only needed for the unstable states
  }
  term_state(done) {
    //nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_config_uri(State& ps, Consumer&& consumer) {
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
void read_config_value(State& ps, Consumer&& consumer, InsideList inside_list) {
  // clang-format off
  start();
  state(init) {
    fsm_epsilon(read_string(ps, consumer), done, quote_marks)
    fsm_epsilon(read_number(ps, consumer), done, '.')
    fsm_epsilon(read_bool(ps, consumer), done, "ft")
    fsm_epsilon(read_number_or_timespan(ps, consumer, inside_list),
                done, "0123456789+-")
    fsm_epsilon(read_config_uri(ps, consumer), done, '<')
    fsm_transition(read_config_list(ps, consumer.begin_list()), done, '[')
    fsm_transition(read_config_map(ps, consumer.begin_map()), done, '{')
  }
  term_state(done) {
    // nop
  }
  fin();
  // clang-format on
}

template <class State, class Consumer>
void read_config(State& ps, Consumer&& consumer) {
  auto key_char = [](char x) {
    return isalnum(x) || x == '-' || x == '_' || x == '"';
  };
  // clang-format off
  start();
  // Checks whether there's a top-level '{'.
  term_state(init) {
    transition(init, " \t\n")
    fsm_epsilon(read_config_comment(ps, consumer), init, '#')
    fsm_transition(read_config_map<false>(ps, consumer),
                   await_closing_brace, '{')
    fsm_epsilon(read_config_map<false>(ps, consumer), init, key_char)
  }
  state(await_closing_brace) {
    transition(await_closing_brace, " \t\n")
    fsm_epsilon(read_config_comment(ps, consumer), await_closing_brace, '#')
    transition(done, '}')
  }
  term_state(done) {
    transition(done, " \t\n")
    fsm_epsilon(read_config_comment(ps, consumer), done, '#')
  }
  fin();
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
