/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2016                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE composition
#include "caf/test/dsl.hpp"

using namespace std;
using namespace caf;

namespace {

behavior multiplier(int x) {
  return {
    [=](int y) {
      return x * y;
    }
  };
}

behavior adder(int x) {
  return {
    [=](int y) {
      return x + y;
    }
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(composition_tests, fixture)

CAF_TEST(depth2) {
  auto stage1 = sys.spawn(multiplier, 4);
  auto stage2 = sys.spawn(adder, 10);
  auto testee = stage2 * stage1;
  self->send(testee, 1);
  expect(int, from(self).to(stage1).with(1));
  expect(int, from(self).to(stage2).with(4));
  CAF_CHECK_EQUAL(fetch_result<int>(), 14);
}

CAF_TEST(depth3) {
  auto stage1 = sys.spawn(multiplier, 4);
  auto stage2 = sys.spawn(adder, 10);
  auto testee = stage1 * stage2 * stage1;
  self->send(testee, 1);
  expect(int, from(self).to(stage1).with(1));
  expect(int, from(self).to(stage2).with(4));
  expect(int, from(self).to(stage1).with(14));
  CAF_CHECK_EQUAL(fetch_result<int>(), 56);
}

CAF_TEST_FIXTURE_SCOPE_END()

