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

#define CAF_SUITE sequencer
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

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
}

second_stage::behavior_type typed_second_stage() {
  return [](double x, double y) {
    return x * y;
  };
}

struct fixture {
  fixture() : system(cfg), self(system, true) {
    // nop
  }

  template <class Actor>
  static bool exited(const Actor& handle) {
    auto ptr = actor_cast<abstract_actor*>(handle);
    auto dptr = dynamic_cast<monitorable_actor*>(ptr);
    CAF_REQUIRE(dptr != nullptr);
    return dptr->getf(abstract_actor::is_terminated_flag);
  }

  actor_system_config cfg;
  actor_system system;
  scoped_actor self;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(sequencer_tests, fixture)

CAF_TEST(identity) {
  actor_system_config cfg_g;
  actor_system system_of_g{cfg_g};
  actor_system_config cfg_f;
  actor_system system_of_f{cfg_f};
  auto g = system_of_g.spawn(typed_first_stage);
  auto f = system_of_f.spawn(typed_second_stage);
  CAF_CHECK_EQUAL(system_of_g.registry().running(), 1u);
  auto h = f * g;
  CAF_CHECK_EQUAL(system_of_g.registry().running(), 1u);
  CAF_CHECK_EQUAL(&h->home_system(), &g->home_system());
  CAF_CHECK_EQUAL(h->node(), g->node());
  CAF_CHECK_NOT_EQUAL(h->id(), g->id());
  CAF_CHECK_NOT_EQUAL(h.address(), g.address());
  CAF_CHECK_EQUAL(h->message_types(), g->home_system().message_types(h));
}

// spawned dead if `g` is already dead upon spawning
CAF_TEST(lifetime_1a) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  anon_send_exit(g, exit_reason::kill);
  self->wait_for(g);
  auto h = f * g;
  CAF_CHECK(exited(h));
}

// spawned dead if `f` is already dead upon spawning
CAF_TEST(lifetime_1b) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  anon_send_exit(f, exit_reason::kill);
  self->wait_for(f);
  auto h = f * g;
  CAF_CHECK(exited(h));
}

// `f.g` exits when `g` exits
CAF_TEST(lifetime_2a) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  self->monitor(h);
  anon_send(g, message{});
}

// `f.g` exits when `f` exits
CAF_TEST(lifetime_2b) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  self->monitor(h);
  anon_send(f, message{});
}

CAF_TEST(request_response_promise) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  auto h = f * g;
  anon_send_exit(h, exit_reason::kill);
  CAF_CHECK(exited(h));
  self->request(h, infinite, 1).receive(
    [](int) {
      CAF_CHECK(false);
    },
    [](error err) {
      CAF_CHECK_EQUAL(err.code(), static_cast<uint8_t>(sec::request_receiver_down));
    }
  );
}

// single composition of distinct actors
CAF_TEST(dot_composition_1) {
  auto first = system.spawn(typed_first_stage);
  auto second = system.spawn(typed_second_stage);
  auto first_then_second = second * first;
  self->request(first_then_second, infinite, 42).receive(
    [](double res) {
      CAF_CHECK_EQUAL(res, (42 * 2.0) * (42 * 4.0));
    },
    ERROR_HANDLER
  );
}

// multiple self composition
CAF_TEST(dot_composition_2) {
  auto dbl_actor = system.spawn(testee);
  auto dbl_x4_actor = dbl_actor * dbl_actor
                      * dbl_actor * dbl_actor;
  self->request(dbl_x4_actor, infinite, 1).receive(
    [](int v) {
      CAF_CHECK_EQUAL(v, 16);
    },
    ERROR_HANDLER
  );
}

CAF_TEST_FIXTURE_SCOPE_END()
