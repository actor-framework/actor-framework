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

#define CAF_SUITE sequencer
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace caf;

namespace {

behavior testee(event_based_actor* self) {
  return {
    [](int v) {
      return 2 * v;
    },
    [=] {
      self->quit();
    }
  };
}

using first_stage = typed_actor<replies_to<int>::with<double, double>>;
using second_stage = typed_actor<replies_to<double, double>::with<double>>;

first_stage::behavior_type typed_first_stage() {
  return [](int i) {
    return std::make_tuple(i * 2.0, i * 4.0);
  };
};

second_stage::behavior_type typed_second_stage() {
  return [](double x, double y) {
    return x * y;
  };
}

struct fixture {
  void wait_until_exited() {
    self->receive(
      [](const down_msg&) {
        CAF_CHECK(true);
      }
    );
  }

  template <class Actor>
  static bool exited(const Actor& handle) {
    auto ptr = actor_cast<abstract_actor*>(handle);
    auto dptr = dynamic_cast<monitorable_actor*>(ptr);
    CAF_REQUIRE(dptr != nullptr);
    return dptr->exited();
  }

  actor_system system;
  scoped_actor self{system, true};
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(sequencer_tests, fixture)

CAF_TEST(identity) {
  actor_system system_of_g;
  actor_system system_of_f;
  auto g = system_of_g.spawn(typed_first_stage);
  auto f = system_of_f.spawn(typed_second_stage);
  CAF_CHECK(system_of_g.registry().running() == 1);
  auto h = f * g;
  CAF_CHECK(system_of_g.registry().running() == 1);
  CAF_CHECK(&h->home_system() == &g->home_system());
  CAF_CHECK(h->node() == g->node());
  CAF_CHECK(h->id() != g->id());
  CAF_CHECK(h != g);
  CAF_CHECK(h->message_types() == g->home_system().message_types(h));
  anon_send_exit(h, exit_reason::kill);
  anon_send_exit(f, exit_reason::kill);
  anon_send_exit(g, exit_reason::kill);
}

// spawned dead if `g` is already dead upon spawning
CAF_TEST(lifetime_1a) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  self->monitor(g);
  anon_send_exit(g, exit_reason::kill);
  wait_until_exited();
  auto h = f * g;
  CAF_CHECK(exited(h));
  anon_send_exit(f, exit_reason::kill);
}

// spawned dead if `f` is already dead upon spawning
CAF_TEST(lifetime_1b) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  self->monitor(f);
  anon_send_exit(f, exit_reason::kill);
  wait_until_exited();
  auto h = f * g;
  CAF_CHECK(exited(h));
  anon_send_exit(g, exit_reason::kill);
}

// `f.g` exits when `g` exits
CAF_TEST(lifetime_2a) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  self->monitor(h);
  anon_send(g, message{});
  wait_until_exited();
  anon_send_exit(f, exit_reason::kill);
}

// `f.g` exits when `f` exits
CAF_TEST(lifetime_2b) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  self->monitor(h);
  anon_send(f, message{});
  wait_until_exited();
  anon_send_exit(g, exit_reason::kill);
}

// 1) ignores down message not from constituent actors
// 2) exits by receiving an exit message
// 3) exit has no effect on constituent actors
CAF_TEST(lifetime_3) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  self->monitor(h);
  anon_send(h, down_msg{self->address(), exit_reason::kill});
  CAF_CHECK(! exited(h));
  auto em_sender = system.spawn(testee);
  em_sender->link_to(h.address());
  anon_send_exit(em_sender, exit_reason::kill);
  wait_until_exited();
  self->request(f, 1).receive(
    [](int v) {
      CAF_CHECK(v == 2);
    },
    [](error) {
      CAF_CHECK(false);
    }
  );
  self->request(g, 1).receive(
    [](int v) {
      CAF_CHECK(v == 2);
    },
    [](error) {
      CAF_CHECK(false);
    }
  );
  anon_send_exit(f, exit_reason::kill);
  anon_send_exit(g, exit_reason::kill);
}

CAF_TEST(request_response_promise) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  anon_send_exit(h, exit_reason::kill);
  CAF_CHECK(exited(h));
  self->request(h, 1).receive(
    [](int) {
      CAF_CHECK(false);
    },
    [](error err) {
      CAF_CHECK(err.code() == static_cast<uint8_t>(sec::request_receiver_down));
    }
  );
  anon_send_exit(f, exit_reason::kill);
  anon_send_exit(g, exit_reason::kill);
}

// single composition of distinct actors
CAF_TEST(dot_composition_1) {
  auto first = system.spawn(typed_first_stage);
  auto second = system.spawn(typed_second_stage);
  auto first_then_second = second * first;
  self->request(first_then_second, 42).receive(
    [](double res) {
      CAF_CHECK(res == (42 * 2.0) * (42 * 4.0));
    }
  );
  anon_send_exit(first, exit_reason::kill);
  anon_send_exit(second, exit_reason::kill);
}

// multiple self composition
CAF_TEST(dot_composition_2) {
  auto dbl_actor = system.spawn(testee);
  auto dbl_x4_actor = dbl_actor * dbl_actor
                      * dbl_actor * dbl_actor;
  self->request(dbl_x4_actor, 1).receive(
    [](int v) {
      CAF_CHECK(v == 16);
    },
    [](error) {
      CAF_CHECK(false);
    }
  );
  anon_send_exit(dbl_actor, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
