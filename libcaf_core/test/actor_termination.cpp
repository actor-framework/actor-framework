/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
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

#include "caf/config.hpp"

// this suite tests whether actors terminate as expect in several use cases
#define CAF_SUITE actor_termination
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

behavior mirror_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return [] {
    // nop
  };
}

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor scoped_self;
  actor mirror;
  actor testee;

  fixture()
      : system(cfg),
        scoped_self(system),
        mirror(system.spawn(mirror_impl)),
        testee(unsafe_actor_handle_init) {
    // nop
  }

  template <class... Ts>
  void spawn(Ts&&... xs) {
    testee = scoped_self->spawn(std::forward<Ts>(xs)...);
  }

  ~fixture() {
    scoped_self->wait_for(testee);
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(actor_termination_tests, fixture)

CAF_TEST(single_multiplexed_request) {
  auto f = [&](event_based_actor* self, actor server) {
    self->request(server, infinite, 42).then(
      [=](int x) {
        CAF_REQUIRE_EQUAL(x, 42);
      }
    );
  };
  spawn(f, mirror);
}

CAF_TEST(multiple_multiplexed_requests) {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->request(server, infinite, 42).then(
        [=](int x) {
          CAF_REQUIRE_EQUAL(x, 42);
        }
      );
  };
  spawn(f, mirror);
}

CAF_TEST(single_awaited_request) {
  auto f = [&](event_based_actor* self, actor server) {
    self->request(server, infinite, 42).await(
      [=](int x) {
        CAF_REQUIRE_EQUAL(x, 42);
      }
    );
  };
  spawn(f, mirror);
}

CAF_TEST(multiple_awaited_requests) {
  auto f = [&](event_based_actor* self, actor server) {
    for (int i = 0; i < 3; ++i)
      self->request(server, infinite, 42).await(
        [=](int x) {
          CAF_MESSAGE("received response #" << (i + 1));
          CAF_REQUIRE_EQUAL(x, 42);
        }
      );
  };
  spawn(f, mirror);
}

CAF_TEST_FIXTURE_SCOPE_END()
