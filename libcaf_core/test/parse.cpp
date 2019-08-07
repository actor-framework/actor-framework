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

#define CAF_SUITE parse

#include "caf/detail/parse.hpp"

#include "caf/test/dsl.hpp"

#include "caf/expected.hpp"
#include "caf/string_view.hpp"

using namespace caf;

namespace {

template <class T>
expected<T> read(string_view str) {
  T result;
  detail::parse_state ps{str.begin(), str.end()};
  detail::parse(ps, result);
  if (ps.code == pec::success)
    return result;
  return ps.code;
}

} // namespace

#define CHECK_NUMBER(type, value)                                              \
  CAF_CHECK_EQUAL(read<type>(#value), type(value))

#define CHECK_NUMBER_3(type, value, cpp_value)                                 \
  CAF_CHECK_EQUAL(read<type>(#value), type(cpp_value))

#define CHECK_INVALID(type, str, code) CAF_CHECK_EQUAL(read<type>(str), code)

CAF_TEST(valid signed integers) {
  CHECK_NUMBER(int8_t, -128);
  CHECK_NUMBER(int8_t, 127);
  CHECK_NUMBER(int8_t, +127);
  CHECK_NUMBER(int16_t, -32768);
  CHECK_NUMBER(int16_t, 32767);
  CHECK_NUMBER(int16_t, +32767);
  CHECK_NUMBER(int32_t, -2147483648);
  CHECK_NUMBER(int32_t, 2147483647);
  CHECK_NUMBER(int32_t, +2147483647);
  CHECK_NUMBER(int64_t, -9223372036854775807);
  CHECK_NUMBER(int64_t, 9223372036854775807);
  CHECK_NUMBER(int64_t, +9223372036854775807);
}

CAF_TEST(invalid signed integers) {
  CHECK_INVALID(int8_t, "--1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "++1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "-129", pec::integer_underflow);
  CHECK_INVALID(int8_t, "128", pec::integer_overflow);
  CHECK_INVALID(int8_t, "~1", pec::unexpected_character);
  CHECK_INVALID(int8_t, "1!", pec::trailing_character);
  CHECK_INVALID(int8_t, "+", pec::unexpected_eof);
  CHECK_INVALID(int8_t, "-", pec::unexpected_eof);
}

CAF_TEST(valid unsigned integers) {
  CHECK_NUMBER(uint8_t, 0);
  CHECK_NUMBER(uint8_t, +0);
  CHECK_NUMBER(uint8_t, 255);
  CHECK_NUMBER(uint8_t, +255);
  CHECK_NUMBER(uint16_t, 0);
  CHECK_NUMBER(uint16_t, +0);
  CHECK_NUMBER(uint16_t, 65535);
  CHECK_NUMBER(uint16_t, +65535);
  CHECK_NUMBER(uint32_t, 0);
  CHECK_NUMBER(uint32_t, +0);
  CHECK_NUMBER(uint32_t, 4294967295);
  CHECK_NUMBER(uint32_t, +4294967295);
  CHECK_NUMBER(uint64_t, 0);
  CHECK_NUMBER(uint64_t, +0);
  CHECK_NUMBER_3(uint64_t, 18446744073709551615, 18446744073709551615ULL);
  CHECK_NUMBER_3(uint64_t, +18446744073709551615, 18446744073709551615ULL);
}

CAF_TEST(invalid unsigned integers) {
  CHECK_INVALID(uint8_t, "-1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "++1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "256", pec::integer_overflow);
  CHECK_INVALID(uint8_t, "~1", pec::unexpected_character);
  CHECK_INVALID(uint8_t, "1!", pec::trailing_character);
  CHECK_INVALID(uint8_t, "+", pec::unexpected_eof);
}

CAF_TEST(valid floating point numbers) {
  CHECK_NUMBER(float, 1);
  CHECK_NUMBER(double, 1);
  CHECK_NUMBER(double, 0.01e10);
  CHECK_NUMBER(double, 10e-10);
  CHECK_NUMBER(double, -10e-10);
}

CAF_TEST(invalid floating point numbers) {
  CHECK_INVALID(float, "1..", pec::trailing_character);
  CHECK_INVALID(double, "..1", pec::unexpected_character);
  CHECK_INVALID(double, "+", pec::unexpected_eof);
  CHECK_INVALID(double, "-", pec::unexpected_eof);
  CHECK_INVALID(double, "1e", pec::unexpected_eof);
  CHECK_INVALID(double, "--0.01e10", pec::unexpected_character);
  CHECK_INVALID(double, "++10e-10", pec::unexpected_character);
}

CAF_TEST(valid atom value) {
  CAF_CHECK_EQUAL(read<atom_value>("foo"), atom("foo"));
  CAF_CHECK_EQUAL(read<atom_value>("'foo'"), atom("foo"));
  CAF_CHECK_EQUAL(read<atom_value>("foooooooooo"), pec::too_many_characters);
  CAF_CHECK_EQUAL(read<atom_value>("foo,bar"), pec::trailing_character);
  CAF_CHECK_EQUAL(read<atom_value>("$"), pec::unexpected_character);
}
