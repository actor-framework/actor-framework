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

#include "caf/pec.hpp"

#include "caf/error.hpp"
#include "caf/make_message.hpp"

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
  "type_mismatch",
  "not_an_option",
  "illegal_argument",
  "missing_argument",
  "illegal_category",
};

} // namespace <anonymous>

namespace caf {

error make_error(pec code) {
  return {static_cast<uint8_t>(code), atom("parser")};
}

error make_error(pec code, size_t line, size_t column) {
  return {static_cast<uint8_t>(code), atom("parser"),
          make_message(static_cast<uint32_t>(line),
                       static_cast<uint32_t>(column))};
}

const char* to_string(pec x) {
  return tbl[static_cast<uint8_t>(x)];
}

} // namespace caf
