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
#include "caf/detail/parser/read_floating_point.hpp"
#include "caf/detail/parser/read_signed_integer.hpp"
#include "caf/detail/parser/read_unsigned_integer.hpp"

#define PARSE_IMPL(type, parser_name)                                          \
  void parse(parse_state& ps, type& x) {                                       \
    parser::read_##parser_name(ps, make_consumer(x));                          \
  }

namespace caf {
namespace detail {

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

} // namespace detail
} // namespace caf
