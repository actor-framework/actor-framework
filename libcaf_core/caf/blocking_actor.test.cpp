// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/blocking_actor.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/intrusive_ptr.hpp"
#include "caf/message.hpp"
#include "caf/others.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;

namespace {

struct fixture : test::fixture::deterministic {
  scoped_actor self{sys};
};

WITH_FIXTURE(fixture) {

TEST("catch_all") {
  self->send(self, 42);
  self->receive([this](float) { fail("received unexpected float"); },
                others >> [this](message& msg) -> skippable_result {
                  check_eq(to_tuple<int32_t>(msg), std::make_tuple(42));
                  return make_error(sec::unexpected_message);
                });
  self->receive(
    [this](const error& err) { check_eq(err, sec::unexpected_message); });
}

TEST("behavior_ref") {
  behavior bhvr{[](int i) { test::runnable::current().check_eq(i, 42); }};
  self->send(self, 42);
  self->receive(bhvr);
}

TEST("timeout_in_scoped_actor") {
  bool timeout_called = false;
  self->receive(after(std::chrono::milliseconds(20)) >>
                [&] { timeout_called = true; });
  check(timeout_called);
}

} // WITH_FIXTURE(fixture)

} // namespace
