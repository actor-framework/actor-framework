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

#include "caf/detail/parser/ec.hpp"

#include "caf/error.hpp"

namespace {

constexpr const char* tbl[] = {
  "success",
  "trailing_character",
  "unexpected_eof",
  "unexpected_character",
  "negative_duration",
  "duration_overflow",
  "too_many_characters",
  "illegal_escape_sequence",
  "unexpected_newline",
  "integer_overflow",
  "integer_underflow",
  "exponent_underflow",
  "exponent_overflow",
};

} // namespace <anonymous>

namespace caf {
namespace detail {
namespace parser {

error make_error(ec code) {
  return {static_cast<uint8_t>(code), atom("parser")};
}

const char* to_string(ec x) {
  return tbl[static_cast<uint8_t>(x)];
}

} // namespace parser
} // namespace detail
} // namespace caf
