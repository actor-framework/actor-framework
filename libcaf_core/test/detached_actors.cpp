// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detached_actors

#include "caf/all.hpp"

#include "core-test.hpp"

using namespace caf;

using std::endl;

namespace {

struct fixture {
  actor_system_config cfg;
  actor_system sys;
  scoped_actor self;

  fixture() : sys(cfg), self(sys, true) {
    // nop
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(detached_actors, fixture)

CAF_TEST(shutdown) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor?");
  sys.spawn<detached>([] {});
}

CAF_TEST(shutdown_with_delayed_send) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "delayed_send?");
  auto f = [](event_based_actor* self) -> behavior {
    self->delayed_send(self, std::chrono::nanoseconds(1), ok_atom_v);
    return {
      [=](ok_atom) { self->quit(); },
    };
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_with_unhandled_delayed_send) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "delayed_send but didn't bother waiting for it?");
  auto f = [](event_based_actor* self) {
    self->delayed_send(self, std::chrono::nanoseconds(1), ok_atom_v);
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_with_after) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "after()?");
  auto f = [](event_based_actor* self) -> behavior {
    return {
      after(std::chrono::nanoseconds(1)) >> [=] { self->quit(); },
    };
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_delayed_send_loop) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "a delayed send loop and was interrupted via exit message?");
  auto f = [](event_based_actor* self) -> behavior {
    self->delayed_send(self, std::chrono::milliseconds(1), ok_atom_v);
    return {
      [=](ok_atom) {
        self->delayed_send(self, std::chrono::milliseconds(1), ok_atom_v);
      },
    };
  };
  auto a = sys.spawn<detached>(f);
  auto g = detail::make_scope_guard(
    [&] { self->send_exit(a, exit_reason::user_shutdown); });
}

CAF_TEST_FIXTURE_SCOPE_END()
