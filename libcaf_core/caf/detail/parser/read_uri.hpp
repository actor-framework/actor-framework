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

#include "caf/config.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_ipv6_address.hpp"
#include "caf/detail/parser/state.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/uri.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf {
namespace detail {
namespace parser {

//   foo://example.com:8042/over/there?name=ferret#nose
//   \_/   \______________/\_________/ \_________/ \__/
//    |           |            |            |        |
// scheme     authority       path        query   fragment
//    |   _____________________|__
//   / \ /                        \.
//   urn:example:animal:ferret:nose

// Unlike our other parsers, the URI parsers only check for validity and
// generate ranges for the subcomponents. URIs can't have linebreaks, so we can
// safely keep track of the position by looking at the column.

template <class Iterator, class Sentinel>
void read_uri_percent_encoded(state<Iterator, Sentinel>& ps, std::string& str) {
  uint8_t char_code = 0;
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      str += static_cast<char>(char_code);
  });
  start();
  state(init) {
    transition(read_nibble, hexadecimal_chars, add_ascii<16>(char_code, ch))
  }
  state(read_nibble) {
    transition(done, hexadecimal_chars, add_ascii<16>(char_code, ch))
  }
  term_state(done) {
    // nop
  }
  fin();
}

inline bool uri_unprotected_char(char c) {
  return in_whitelist(alphanumeric_chars, c) || in_whitelist("-._~", c);
}

#define read_next_char(next_state, dest)                                       \
  transition(next_state, uri_unprotected_char, dest += ch)                     \
  fsm_transition(read_uri_percent_encoded(ps, dest), next_state, '%')

template <class Iterator, class Sentinel, class Consumer>
void read_uri_query(state<Iterator, Sentinel>& ps, Consumer&& consumer) {
  // Local variables.
  uri::query_map result;
  std::string key;
  std::string value;
  // Utility functions.
  auto take_str = [&](std::string& str) {
    using std::swap;
    std::string res;
    swap(str, res);
    return std::move(res);
  };
  auto push = [&] {
    result.emplace(take_str(key), take_str(value));
  };
  // Call consumer on exit.
  auto g = make_scope_guard([&] {
    if (ps.code <= pec::trailing_character)
      consumer.query(std::move(result));
  });
  // FSM declaration.
  start();
  // Query may be empty.
  term_state(init) {
    read_next_char(read_key, key)
  }
  state(read_key) {
    read_next_char(read_key, key)
    transition(read_value, '=')
  }
  term_state(read_value, push()) {
    read_next_char(read_value, value)
    transition(init, '&', push())
  }
  fin();
}

template <class Iterator, class Sentinel, class Consumer>
void read_uri(state<Iterator, Sentinel>& ps, Consumer&& consumer) {
  // Local variables.
  std::string str;
  uint16_t port = 0;
  // Replaces `str` with a default constructed string to make sure we're never
  // operating on a moved-from string object.
  auto take_str = [&] {
    using std::swap;
    std::string res;
    swap(str, res);
    return std::move(res);
  };
  // Allowed character sets.
  auto path_char = [](char c) {
    return in_whitelist(alphanumeric_chars, c) || c == '/';
  };
  // Utility setters for avoiding code duplication.
  auto set_path = [&] {
    consumer.path(take_str());
  };
  auto set_host = [&] {
    consumer.host(take_str());
  };
  auto set_userinfo = [&] {
    consumer.userinfo(take_str());
  };
  // Consumer for reading IPv6 addresses.
  struct {
    Consumer& f;
    void value(ipv6_address addr) {
      f.host(addr);
    }
  } ip_consumer{consumer};
  // FSM declaration.
  start();
  state(init) {
    epsilon(read_scheme)
  }
  state(read_scheme) {
    read_next_char(read_scheme, str)
    transition(have_scheme, ':', consumer.scheme(take_str()))
  }
  state(have_scheme) {
    transition(disambiguate_path, '/')
    read_next_char(read_path, str)
    fsm_transition(read_uri_percent_encoded(ps, str), read_path, '%')
  }
  // This state is terminal, because "file:/" is a valid URI.
  term_state(disambiguate_path, consumer.path("/")) {
    transition(start_authority, '/')
    epsilon(read_path, any_char, str += '/')
  }
  state(start_authority) {
    read_next_char(read_authority, str)
    fsm_transition(read_ipv6_address(ps, ip_consumer), await_end_of_ipv6, '[')
  }
  state(await_end_of_ipv6) {
    transition(end_of_ipv6_host, ']')
  }
  term_state(end_of_ipv6_host) {
    transition(start_port, ':')
    epsilon(end_of_authority)
  }
  term_state(end_of_host) {
    transition(start_port, ':', set_host())
    epsilon(end_of_authority, "/?#", set_host())
  }
  term_state(read_authority, set_host()) {
    read_next_char(read_authority, str)
    transition(start_host, '@', set_userinfo())
    transition(start_port, ':', set_host())
    epsilon(end_of_authority, "/?#", set_host())
  }
  state(start_host) {
    read_next_char(read_host, str)
    fsm_transition(read_ipv6_address(ps, ip_consumer), await_end_of_ipv6, '[')
  }
  term_state(read_host, set_host()) {
    read_next_char(read_host, str)
    transition(start_port, ':', set_host())
    epsilon(end_of_authority, "/?#", set_host())
  }
  state(start_port) {
    transition(read_port, decimal_chars, add_ascii<10>(port, ch))
  }
  term_state(read_port, consumer.port(port)) {
    transition(read_port, decimal_chars, add_ascii<10>(port, ch),
               pec::integer_overflow)
    epsilon(end_of_authority, "/?#", consumer.port(port))
  }
  term_state(end_of_authority) {
    transition(read_path, '/')
    transition(start_query, '?')
    transition(read_fragment, '#')
  }
  term_state(read_path, set_path()) {
    transition(read_path, path_char, str += ch)
    fsm_transition(read_uri_percent_encoded(ps, str), read_path, '%')
    transition(start_query, '?', set_path())
    transition(read_fragment, '#', set_path())
  }
  term_state(start_query) {
    fsm_epsilon(read_uri_query(ps, consumer), end_of_query)
  }
  unstable_state(end_of_query) {
    transition(read_fragment, '#')
    epsilon(done)
  }
  term_state(read_fragment, consumer.fragment(take_str())) {
    read_next_char(read_fragment, str)
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
