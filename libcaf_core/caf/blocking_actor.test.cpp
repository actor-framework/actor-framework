// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/blocking_actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/message.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

void testee(blocking_actor* self) {
  self->receive([](int i) { return i; });
}

struct fixture {
  fixture() : sys(adjust(cfg)), self(sys) {
    // nop
  }

  actor_system_config cfg;

  actor_system sys;

  scoped_actor self;

  static actor_system_config& adjust(actor_system_config& what) {
    put(what.content, "caf.scheduler.max-threads", 1u);
    return what;
  }
};

} // namespace

WITH_FIXTURE(fixture) {

TEST("message catch-all handler") {
  self->mail(42).send(self);
  self->receive([this](float) { fail("received unexpected float"); },
                [this](message& msg) {
                  check_eq(to_tuple<int32_t>(msg), std::make_tuple(42));
                  return make_error(sec::unexpected_message);
                });
  self->receive(
    [this](const error& err) { check_eq(err, sec::unexpected_message); });
}

TEST("behavior_ref") {
  behavior bhvr{[](int i) { test::runnable::current().check_eq(i, 42); }};
  self->mail(42).send(self);
  self->receive(bhvr);
}

TEST("timeout_in_scoped_actor") {
  bool timeout_called = false;
  self->receive(after(std::chrono::milliseconds(20)) >>
                [&] { timeout_called = true; });
  check(timeout_called);
}

// Blocking actors cannot be spawned in deterministic test mode (they require
// detach_flag). This test uses a normal actor system.
TEST("spawn blocking actor") {
  auto aut = sys.spawn(testee);
  self->mail(42).send(aut);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](int i) {
    *received = true;
    check_eq(i, 42);
  });
  check(*received);
}

} // WITH_FIXTURE(fixture)
