// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE decorator.sequencer

#include "caf/decorator/sequencer.hpp"

#include "core-test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER [&](error& err) { CAF_FAIL(err); }

CAF_PUSH_DEPRECATED_WARNING

using namespace caf;

namespace {

behavior testee(event_based_actor* self) {
  return {
    [](int v) { return 2 * v; },
    [=] { self->quit(); },
  };
}

using first_stage = typed_actor<result<double, double>(int)>;
using second_stage = typed_actor<result<double>(double, double)>;

first_stage::behavior_type typed_first_stage() {
  return {
    [](int i) -> result<double, double> {
      return {i * 2.0, i * 4.0};
    },
  };
}

second_stage::behavior_type typed_second_stage() {
  return {
    [](double x, double y) { return x * y; },
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

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

CAF_TEST(identity) {
  actor_system_config cfg_g;
  actor_system system_of_g{cfg_g};
  actor_system_config cfg_f;
  actor_system system_of_f{cfg_f};
  auto g = system_of_g.spawn(typed_first_stage);
  auto f = system_of_f.spawn(typed_second_stage);
  CHECK_EQ(system_of_g.registry().running(), 1u);
  auto h = f * g;
  CHECK_EQ(system_of_g.registry().running(), 1u);
  CHECK_EQ(&h->home_system(), &g->home_system());
  CHECK_EQ(h->node(), g->node());
  CHECK_NE(h->id(), g->id());
  CHECK_NE(h.address(), g.address());
  CHECK_EQ(h->message_types(), g->home_system().message_types(h));
}

// spawned dead if `g` is already dead upon spawning
CAF_TEST(lifetime_1a) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  anon_send_exit(g, exit_reason::kill);
  self->wait_for(g);
  auto h = f * g;
  CHECK(exited(h));
}

// spawned dead if `f` is already dead upon spawning
CAF_TEST(lifetime_1b) {
  auto g = system.spawn(testee);
  auto f = system.spawn(testee);
  anon_send_exit(f, exit_reason::kill);
  self->wait_for(f);
  auto h = f * g;
  CHECK(exited(h));
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
  CHECK(exited(h));
  self->request(h, infinite, 1)
    .receive([](int) { CHECK(false); },
             [](error err) {
               CHECK_EQ(err.code(),
                        static_cast<uint8_t>(sec::request_receiver_down));
             });
}

// single composition of distinct actors
CAF_TEST(dot_composition_1) {
  auto first = system.spawn(typed_first_stage);
  auto second = system.spawn(typed_second_stage);
  auto first_then_second = second * first;
  self->request(first_then_second, infinite, 42)
    .receive([](double res) { CHECK_EQ(res, (42 * 2.0) * (42 * 4.0)); },
             ERROR_HANDLER);
}

// multiple self composition
CAF_TEST(dot_composition_2) {
  auto dbl_actor = system.spawn(testee);
  auto dbl_x4_actor = dbl_actor * dbl_actor * dbl_actor * dbl_actor;
  self->request(dbl_x4_actor, infinite, 1)
    .receive([](int v) { CHECK_EQ(v, 16); }, ERROR_HANDLER);
}

END_FIXTURE_SCOPE()

CAF_POP_WARNINGS
