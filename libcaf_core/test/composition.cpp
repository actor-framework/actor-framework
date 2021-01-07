// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE composition

#include "caf/actor.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

behavior multiplier(int x) {
  return {
    [=](int y) {
      return x * y;
    },
    [=](int y1, int y2) {
      return x * y1 * y2;
    }
  };
}

behavior adder(int x) {
  return {
    [=](int y) {
      return x + y;
    },
    [=](int y1, int y2) {
      return x + y1 + y2;
    }
  };
}

behavior float_adder(float x) {
  return {
    [=](float y) {
      return x + y;
    }
  };
}

using fixture = test_coordinator_fixture<>;

} // namespace

CAF_TEST_FIXTURE_SCOPE(composition_tests, fixture)

CAF_TEST(depth2) {
  auto stage1 = sys.spawn(multiplier, 4);
  auto stage2 = sys.spawn(adder, 10);
  auto testee = stage2 * stage1;
  self->send(testee, 1);
  expect((int), from(self).to(stage1).with(1));
  expect((int), from(self).to(stage2).with(4));
  expect((int), from(stage2).to(self).with(14));
}

CAF_TEST(depth3) {
  auto stage1 = sys.spawn(multiplier, 4);
  auto stage2 = sys.spawn(adder, 10);
  auto testee = stage1 * stage2 * stage1;
  self->send(testee, 1);
  expect((int), from(self).to(stage1).with(1));
  expect((int), from(self).to(stage2).with(4));
  expect((int), from(self).to(stage1).with(14));
  expect((int), from(stage1).to(self).with(56));
}

CAF_TEST(depth2_type_mismatch) {
  auto stage1 = sys.spawn(multiplier, 4);
  auto stage2 = sys.spawn(float_adder, 10);
  auto testee = stage2 * stage1;
  self->send(testee, 1);
  expect((int), from(self).to(stage1).with(1));
  expect((int), from(self).to(stage2).with(4));
  expect((error), from(stage2).to(self).with(sec::unexpected_message));
}

CAF_TEST_FIXTURE_SCOPE_END()
