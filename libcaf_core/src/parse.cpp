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
#include "caf/detail/parser/read_atom.hpp"
#include "caf/detail/parser/read_bool.hpp"
#include "caf/detail/parser/read_floating_point.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/parser/read_string.hpp"
#include "caf/detail/parser/read_timespan.hpp"
#include "caf/detail/parser/read_unsigned_integer.hpp"
#include "caf/detail/parser/read_uri.hpp"
#include "caf/uri_builder.hpp"

#define PARSE_IMPL(type, parser_name)                                          \
  void parse(parse_state& ps, type& x) {                                       \
    parser::read_##parser_name(ps, make_consumer(x));                          \
  }

namespace caf {
namespace detail {

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

void parse(parse_state& ps, atom_value& x) {
  parser::read_atom(ps, make_consumer(x), true);
}

void parse(parse_state& ps, uri& x) {
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
    read_uri(ps, builder);
  }
  if (ps.code <= pec::trailing_character)
    x = builder.make();
}

void parse(parse_state& ps, std::string& x) {
  ps.skip_whitespaces();
  if (ps.current() == '"') {
    parser::read_string(ps, make_consumer(x));
    return;
  }
  auto c = ps.current();
  while (c != '\0' && (isalnum(c) || isspace(c))) {
    x += c;
    c = ps.next();
  }
  while (!x.empty() && isspace(x.back()))
    x.pop_back();
  ps.code = ps.at_end() ? pec::success : pec::trailing_character;
}

} // namespace detail
} // namespace caf
