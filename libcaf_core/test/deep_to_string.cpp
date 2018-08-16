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

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

void foobar() {
  // nop
}

} // namespace <anonymous>

CAF_TEST(timespans) {
  CAF_CHECK_EQUAL(deep_to_string(timespan{1}), "1ns");
  CAF_CHECK_EQUAL(deep_to_string(timespan{1000}), "1us");
  CAF_CHECK_EQUAL(deep_to_string(timespan{1000000}), "1ms");
  CAF_CHECK_EQUAL(deep_to_string(timespan{1000000000}), "1s");
  CAF_CHECK_EQUAL(deep_to_string(timespan{60000000000}), "1min");
}

CAF_TEST(pointers) {
  auto i = 42;
  CAF_CHECK_EQUAL(deep_to_string(&i), "*42");
  CAF_CHECK_EQUAL(deep_to_string(foobar), "<fun>");
}
