// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#pragma once

#include "caf/config.hpp"
#include "caf/detail/parser/add_ascii.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_ipv6_address.hpp"
#include "caf/detail/scope_guard.hpp"
#include "caf/pec.hpp"
#include "caf/uri.hpp"

CAF_PUSH_UNUSED_LABEL_WARNING

#include "caf/detail/parser/fsm.hpp"

namespace caf::detail::parser {

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

template <class State>
void read_uri_percent_encoded(State& ps, std::string& str) {
  uint8_t char_code = 0;
  // clang-format off
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
  // clang-format on
  if (ps.code <= pec::trailing_character)
    str += static_cast<char>(char_code);
}

inline bool uri_unprotected_char(char c) noexcept {
  // Consider valid characters not explicitly stated as reserved as unreserved.
  return isprint(c) && !in_whitelist(":/?#[]@!$&'()*+,;=<>", c);
}

// clang-format off
#define read_next_char(next_state, dest)                                       \
  transition(next_state, uri_unprotected_char, dest += ch)                     \
  fsm_transition(read_uri_percent_encoded(ps, dest), next_state, '%')
// clang-format on

template <class State, class Consumer>
void read_uri_query(State& ps, Consumer&& consumer) {
  // Local variables.
  uri::query_map result;
  std::string key;
  std::string value;
  // Utility functions.
  auto take_str = [&](std::string& str) {
    using std::swap;
    std::string res;
    swap(str, res);
    return res;
  };
  auto push = [&] { result.emplace(take_str(key), take_str(value)); };
  // clang-format off
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
  // clang-format on
  if (ps.code <= pec::trailing_character)
    consumer.query(std::move(result));
}

template <class State, class Consumer>
void read_uri(State& ps, Consumer&& consumer) {
  // Local variables.
  std::string str;
  auto colon_position = std::string::npos;
  uint16_t port = 0;
  // Replaces `str` with a default constructed string to make sure we're never
  // operating on a moved-from string object.
  auto take_str = [&] {
    using std::swap;
    std::string res;
    swap(str, res);
    return res;
  };
  // Allowed character sets.
  auto path_char = [](char c) {
    return uri_unprotected_char(c) || c == '/' || c == ':';
  };
  // Utility setters for avoiding code duplication.
  auto set_path = [&] { consumer.path(take_str()); };
  auto set_host = [&] { consumer.host(take_str()); };
  auto set_userinfo = [&] {
    auto str = take_str();
    if (colon_position == std::string::npos) {
      consumer.userinfo(std::move(str));
    } else {
      auto password_part = str.substr(colon_position + 1);
      str.erase(colon_position, std::string::npos);
      consumer.userinfo(std::move(str), std::move(password_part));
    }
  };
  auto on_colon = [&] {
    colon_position = str.size();
    str.push_back(':');
  };
  auto set_host_and_port = [&]() -> pec {
    auto str = take_str();
    auto port_str = std::string_view{str}.substr(colon_position + 1);
    string_parser_state port_ps{port_str.begin(), port_str.end()};
    parse(port_ps, port);
    if (port_ps.code != pec::success) {
      return port_ps.code;
    }
    consumer.port(port);
    str.erase(colon_position, std::string::npos);
    consumer.host(std::move(str));
    return pec::success;
  };
  // Consumer for reading IPv6 addresses.
  struct {
    Consumer& f;
    void value(ipv6_address addr) {
      f.host(addr);
    }
  } ip_consumer{consumer};
  // clang-format off
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
    // A third '/' skips the authority, e.g., "file:///".
    transition(read_path, '/', str += '/')
    read_next_char(read_authority, str)
    transition(read_authority, ':', on_colon())
    transition(start_host, '@')
    fsm_transition(read_ipv6_address(ps, ip_consumer), await_end_of_ipv6, '[')
  }
  state(await_end_of_ipv6) {
    transition(end_of_ipv6_host, ']')
  }
  term_state(end_of_ipv6_host) {
    transition(start_port, ':')
    epsilon(end_of_authority)
  }
  term_state(read_authority, set_host()) {
    read_next_char(read_authority, str)
    transition(start_host, '@', set_userinfo())
    // A ':' can signalize end of userinfo or end of host,
    // e.g., "user:pass@example.com" or "example.com:80".
    transition(read_host_or_port, ':', on_colon())
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
  term_state(read_host_or_port, set_host_and_port()) {
    read_next_char(read_host_or_port, str)
    transition(start_host, '@', set_userinfo())
    epsilon(end_of_authority, "/?#", set_host_and_port())
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
  // clang-format on
}

} // namespace caf::detail::parser

#include "caf/detail/parser/fsm_undef.hpp"

CAF_POP_WARNINGS
