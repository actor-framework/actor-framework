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

#include "caf/detail/parse.hpp"

#include "caf/detail/consumer.hpp"
#include "caf/detail/parser/chars.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_floating_point.hpp"
#include "caf/detail/parser/read_ipv4_address.hpp"
#include "caf/detail/parser/read_ipv6_address.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/parser/read_timespan.hpp"
#include "caf/detail/parser/read_unsigned_integer.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/ipv4_address.hpp"
#include "caf/ipv4_endpoint.hpp"
#include "caf/ipv4_subnet.hpp"
#include "caf/ipv6_address.hpp"
#include "caf/ipv6_endpoint.hpp"
#include "caf/ipv6_subnet.hpp"
#include "caf/uri_builder.hpp"

#define PARSE_IMPL(type, parser_name)                                          \
  void parse(string_parser_state& ps, type& x) {                               \
    parser::read_##parser_name(ps, make_consumer(x));                          \
  }

namespace caf::detail {

struct literal {
  string_view str;

  template <size_t N>
  literal(const char (&cstr)[N]) : str(cstr, N - 1) {
    // nop
  }
};

void parse(string_parser_state& ps, literal& x) {
  CAF_ASSERT(x.str.size() > 0);
  if (ps.current() != x.str[0]) {
    ps.code = pec::unexpected_character;
    return;
  }
  auto c = ps.next();
  for (auto i = x.str.begin() + 1; i != x.str.end(); ++i) {
    if (c != *i) {
      ps.code = pec::unexpected_character;
      return;
    }
    c = ps.next();
  }
  ps.code = ps.at_end() ? pec::success : pec::trailing_character;
}

void parse_sequence(string_parser_state&) {
  // End of recursion.
}

template <class T, class... Ts>
void parse_sequence(string_parser_state& ps, T&& x, Ts&&... xs) {
  parse(ps, x);
  // TODO: use `if constexpr` when switching to C++17
  if (sizeof...(Ts) > 0) {
    CAF_ASSERT(ps.code != pec::success);
    if (ps.code == pec::trailing_character)
      parse_sequence(ps, std::forward<Ts>(xs)...);
  }
}

PARSE_IMPL(bool, bool)

PARSE_IMPL(int8_t, signed_integer)

PARSE_IMPL(int16_t, signed_integer)

PARSE_IMPL(int32_t, signed_integer)

PARSE_IMPL(int64_t, signed_integer)

PARSE_IMPL(uint8_t, unsigned_integer)

PARSE_IMPL(uint16_t, unsigned_integer)

PARSE_IMPL(uint32_t, unsigned_integer)

PARSE_IMPL(uint64_t, unsigned_integer)

PARSE_IMPL(float, floating_point)

PARSE_IMPL(double, floating_point)

PARSE_IMPL(timespan, timespan)

void parse(string_parser_state& ps, uri& x) {
  uri_builder builder;
  if (ps.consume('<')) {
    parser::read_uri(ps, builder);
    if (ps.code > pec::trailing_character)
      return;
    if (!ps.consume('>')) {
      ps.code = pec::unexpected_character;
      return;
    }
  } else {
    parser::read_uri(ps, builder);
  }
  if (ps.code <= pec::trailing_character)
    x = builder.make();
}

PARSE_IMPL(ipv4_address, ipv4_address)

void parse(string_parser_state& ps, ipv4_subnet& x) {
  ipv4_address addr;
  uint8_t prefix_length;
  parse_sequence(ps, addr, literal{"/"}, prefix_length);
  if (ps.code <= pec::trailing_character) {
    if (prefix_length > 32) {
      ps.code = pec::integer_overflow;
      return;
    }
    x = ipv4_subnet{addr, prefix_length};
  }
}

void parse(string_parser_state& ps, ipv4_endpoint& x) {
  ipv4_address addr;
  uint16_t port;
  parse_sequence(ps, addr, literal{":"}, port);
  if (ps.code <= pec::trailing_character)
    x = ipv4_endpoint{addr, port};
}

PARSE_IMPL(ipv6_address, ipv6_address)

void parse(string_parser_state& ps, ipv6_subnet& x) {
  // TODO: this algorithm is currently not one-pass. The reason we need to
  // check whether the input is a valid ipv4_subnet first is that "1.2.3.0" is
  // a valid IPv6 address, but "1.2.3.0/16" results in the wrong subnet when
  // blindly reading the address as an IPv6 address.
  auto nested = ps;
  ipv4_subnet v4_subnet;
  parse(nested, v4_subnet);
  if (nested.code <= pec::trailing_character) {
    ps.i = nested.i;
    ps.code = nested.code;
    ps.line = nested.line;
    ps.column = nested.column;
    x = ipv6_subnet{v4_subnet};
    return;
  }
  ipv6_address addr;
  uint8_t prefix_length;
  parse_sequence(ps, addr, literal{"/"}, prefix_length);
  if (ps.code <= pec::trailing_character) {
    if (prefix_length > 128) {
      ps.code = pec::integer_overflow;
      return;
    }
    x = ipv6_subnet{addr, prefix_length};
  }
}

void parse(string_parser_state& ps, ipv6_endpoint& x) {
  ipv6_address addr;
  uint16_t port;
  if (ps.consume('[')) {
    parse_sequence(ps, addr, literal{"]:"}, port);
  } else {
    ipv4_address v4_addr;
    parse_sequence(ps, v4_addr, literal{":"}, port);
    if (ps.code <= pec::trailing_character)
      addr = ipv6_address{v4_addr};
  }
  if (ps.code <= pec::trailing_character)
    x = ipv6_endpoint{addr, port};
}

void parse(string_parser_state& ps, std::string& x) {
  ps.skip_whitespaces();
  if (ps.current() == '"') {
    parser::read_string(ps, make_consumer(x));
    return;
  }
  for (auto c = ps.current(); c != '\0'; c = ps.next())
    x += c;
  while (!x.empty() && isspace(x.back()))
    x.pop_back();
  ps.code = pec::success;
}

void parse_element(string_parser_state& ps, std::string& x,
                   const char* char_blacklist) {
  ps.skip_whitespaces();
  if (ps.current() == '"') {
    parser::read_string(ps, make_consumer(x));
    return;
  }
  auto is_legal
    = [=](char c) { return c != '\0' && strchr(char_blacklist, c) == nullptr; };
  for (auto c = ps.current(); is_legal(c); c = ps.next())
    x += c;
  while (!x.empty() && isspace(x.back()))
    x.pop_back();
  ps.code = ps.at_end() ? pec::success : pec::trailing_character;
}

} // namespace caf::detail
