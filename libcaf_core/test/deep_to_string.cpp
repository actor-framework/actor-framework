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

#define CAF_SUITE deep_to_string

#include "caf/deep_to_string.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

void foobar() {
  // nop
}

} // namespace

#define CHECK_DEEP_TO_STRING(val, str) CAF_CHECK_EQUAL(deep_to_string(val), str)

CAF_TEST(timespans) {
  CHECK_DEEP_TO_STRING(timespan{1}, "1ns");
  CHECK_DEEP_TO_STRING(timespan{1000}, "1us");
  CHECK_DEEP_TO_STRING(timespan{1000000}, "1ms");
  CHECK_DEEP_TO_STRING(timespan{1000000000}, "1s");
  CHECK_DEEP_TO_STRING(timespan{60000000000}, "1min");
}

CAF_TEST(integer lists) {
  int carray[] = {1, 2, 3, 4};
  using array_type = std::array<int, 4>;
  CHECK_DEEP_TO_STRING(std::list<int>({1, 2, 3, 4}), "[1, 2, 3, 4]");
  CHECK_DEEP_TO_STRING(std::vector<int>({1, 2, 3, 4}), "[1, 2, 3, 4]");
  CHECK_DEEP_TO_STRING(std::set<int>({1, 2, 3, 4}), "[1, 2, 3, 4]");
  CHECK_DEEP_TO_STRING(array_type({{1, 2, 3, 4}}), "[1, 2, 3, 4]");
  CHECK_DEEP_TO_STRING(carray, "[1, 2, 3, 4]");
}

CAF_TEST(boolean lists) {
  bool carray[] = {false, true};
  using array_type = std::array<bool, 2>;
  CHECK_DEEP_TO_STRING(std::list<bool>({false, true}), "[false, true]");
  CHECK_DEEP_TO_STRING(std::vector<bool>({false, true}), "[false, true]");
  CHECK_DEEP_TO_STRING(std::set<bool>({false, true}), "[false, true]");
  CHECK_DEEP_TO_STRING(array_type({{false, true}}), "[false, true]");
  CHECK_DEEP_TO_STRING(carray, "[false, true]");
}

CAF_TEST(pointers) {
  auto i = 42;
  CHECK_DEEP_TO_STRING(&i, "*42");
  CHECK_DEEP_TO_STRING(foobar, "<fun>");
}

CAF_TEST(buffers) {
  // Use `signed char` explicitly to make sure all compilers agree.
  std::vector<signed char> buf;
  CAF_CHECK_EQUAL(deep_to_string(buf), "[]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "<empty>");
  buf.push_back(-1);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF");
  buf.push_back(0);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF00");
  buf.push_back(127);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F");
  buf.push_back(10);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127, 10]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F0A");
  buf.push_back(16);
  CAF_CHECK_EQUAL(deep_to_string(buf), "[-1, 0, 127, 10, 16]");
  CAF_CHECK_EQUAL(deep_to_string(meta::hex_formatted(), buf), "FF007F0A10");
}
