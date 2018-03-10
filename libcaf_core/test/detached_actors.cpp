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

#include "caf/config.hpp"

#define CAF_SUITE detached_actors
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

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

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(detached_actors, fixture)

CAF_TEST(shutdown) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor?");
  sys.spawn<detached>([] {});
}

CAF_TEST(shutdown_with_delayed_send) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "delayed_send?");
  auto f = [](event_based_actor* self) -> behavior {
    self->delayed_send(self, std::chrono::nanoseconds(1), ok_atom::value);
    return {
      [=](ok_atom) {
        self->quit();
      }
    };
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_with_unhandled_delayed_send) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "delayed_send but didn't bother waiting for it?");
  auto f = [](event_based_actor* self) {
    self->delayed_send(self, std::chrono::nanoseconds(1), ok_atom::value);
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_with_after) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "after()?");
  auto f = [](event_based_actor* self) -> behavior {
    return {
      after(std::chrono::nanoseconds(1)) >> [=] {
        self->quit();
      }
    };
  };
  sys.spawn<detached>(f);
}

CAF_TEST(shutdown_delayed_send_loop) {
  CAF_MESSAGE("does sys shut down after spawning a detached actor that used "
              "a delayed send loop and was interrupted via exit message?");
  auto f = [](event_based_actor* self) -> behavior {
    self->send(self, std::chrono::milliseconds(1), ok_atom::value);
    return {
      [=](ok_atom) {
        self->send(self, std::chrono::milliseconds(1), ok_atom::value);
      }
    };
  };
  auto a = sys.spawn<detached>(f);
  auto g = detail::make_scope_guard([&] {
    self->send_exit(a, exit_reason::user_shutdown);
  });
}

CAF_TEST_FIXTURE_SCOPE_END()
