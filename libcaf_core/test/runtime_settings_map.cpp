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

#define CAF_SUITE runtime_settings_map

#include "caf/runtime_settings_map.hpp"

#include "caf/test/dsl.hpp"

using namespace caf;

namespace {

void my_fun() {
  // nop
}

struct fixture {
  runtime_settings_map rsm;
  void (*funptr)() = my_fun;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(runtime_settings_map_tests, fixture)

CAF_TEST(get empty) {
  CAF_CHECK(rsm.empty());
  CAF_CHECK_EQUAL(rsm.get(atom("foo")), none);
  rsm.set(atom("foo"), atom("bar"));
  CAF_CHECK(!rsm.empty());
  CAF_CHECK_EQUAL(rsm.get(atom("foo")), atom("bar"));
  rsm.set(atom("foo"), funptr);
  CAF_CHECK_EQUAL(rsm.get(atom("foo")), funptr);
  rsm.set(atom("foo"), none);
  CAF_CHECK_EQUAL(rsm.size(), 0u);
  CAF_CHECK(rsm.empty());
}

CAF_TEST_FIXTURE_SCOPE_END()
