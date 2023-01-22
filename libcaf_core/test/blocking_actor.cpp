// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE blocking_actor

#include "caf/blocking_actor.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;

  fixture() : system(cfg), self(system) {
    // nop
  }
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(catch_all) {
  self->send(self, 42);
  self->receive([](float) { CAF_FAIL("received unexpected float"); },
                others >> [](message& msg) -> skippable_result {
                  CHECK_EQ(to_tuple<int32_t>(msg), std::make_tuple(42));
                  return make_error(sec::unexpected_message);
                });
  self->receive(
    [](const error& err) { CHECK_EQ(err, sec::unexpected_message); });
}

CAF_TEST(behavior_ref) {
  behavior bhvr{[](int i) { CHECK_EQ(i, 42); }};
  self->send(self, 42);
  self->receive(bhvr);
}

CAF_TEST(timeout_in_scoped_actor) {
  bool timeout_called = false;
  scoped_actor self{system};
  self->receive(after(std::chrono::milliseconds(20)) >>
                [&] { timeout_called = true; });
  CHECK(timeout_called);
}

END_FIXTURE_SCOPE()
