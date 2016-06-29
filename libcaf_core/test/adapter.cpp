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

#define CAF_SUITE adapter
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

struct fixture {
  template <class Actor>
  static bool exited(const Actor& handle) {
    auto ptr = actor_cast<abstract_actor*>(handle);
    auto dptr = dynamic_cast<monitorable_actor*>(ptr);
    CAF_REQUIRE(dptr != nullptr);
    return dptr->is_terminated();
  }

  fixture() : system(cfg) {
    // nop
  }

  actor_system_config cfg;
  actor_system system;
  scoped_actor self{system, true};
};

void handle_err(const error& err) {
  throw std::runtime_error("AUT responded with an error: " + to_string(err));
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(adapter_tests, fixture)

CAF_TEST(identity) {
  auto dbl = system.spawn(testee);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  auto bound = dbl.bind(1);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  CAF_CHECK_EQUAL(&bound->home_system(), &dbl->home_system());
  CAF_CHECK_EQUAL(bound->node(), dbl->node());
  CAF_CHECK_NOT_EQUAL(bound, dbl);
  CAF_CHECK_NOT_EQUAL(bound->id(), dbl->id());
  //anon_send_exit(bound, exit_reason::kill);
  anon_send_exit(dbl, exit_reason::kill);
  // killing dbl triggers a down message to bound, which becomes unreachable
  // when it no longer monitors dbl as a result and goes out of scope here
}

// bound actor spawned dead if decorated
// actor is already dead upon spawning
CAF_TEST(lifetime_1) {
  auto dbl = system.spawn(testee);
  self->monitor(dbl);
  anon_send_exit(dbl, exit_reason::kill);
  self->wait_for(dbl);
  auto bound = dbl.bind(1);
  CAF_CHECK(exited(bound));
}

// bound actor exits when decorated actor exits
CAF_TEST(lifetime_2) {
  auto dbl = system.spawn(testee);
  auto bound = dbl.bind(1);
  self->monitor(bound);
  anon_send(dbl, message{});
  self->wait_for(dbl);
}

CAF_TEST(request_response_promise) {
  auto dbl = system.spawn(testee);
  auto bound = dbl.bind(1);
  anon_send_exit(bound, exit_reason::kill);
  CAF_CHECK(exited(bound));
  self->request(bound, infinite, message{}).receive(
    [](int) {
      throw std::runtime_error("received unexpected integer");
    },
    [](error err) {
      CAF_CHECK_EQUAL(err.code(),
                      static_cast<uint8_t>(sec::request_receiver_down));
    }
  );
  anon_send_exit(dbl, exit_reason::kill);
}

CAF_TEST(partial_currying) {
  using namespace std::placeholders;
  auto impl = []() -> behavior {
    return {
      [](ok_atom, int x) {
        return x;
      },
      [](ok_atom, double x) {
        return x;
      }
    };
  };
  auto aut = system.spawn(impl);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK_NOT_EQUAL(aut.id(), bound.id());
  CAF_CHECK_NOT_EQUAL(aut, bound);
  CAF_CHECK_EQUAL(aut.node(), bound.node());
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  self->request(bound, infinite, 2.0).receive(
    [](double y) {
      CAF_CHECK_EQUAL(y, 2.0);
    },
    handle_err
  );
  self->request(bound, infinite, 10).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, 10);
    },
    handle_err
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST(full_currying) {
  auto dbl_actor = system.spawn(testee);
  auto bound = dbl_actor.bind(1);
  self->request(bound, infinite, message{}).receive(
    [](int v) {
      CAF_CHECK_EQUAL(v, 2);
    },
    handle_err
  );
  anon_send_exit(bound, exit_reason::kill);
  anon_send_exit(dbl_actor, exit_reason::kill);
}

CAF_TEST(type_safe_currying) {
  using namespace std::placeholders;
  using testee = typed_actor<replies_to<ok_atom, int>::with<int>,
                             replies_to<ok_atom, double>::with<double>>;
  auto impl = []() -> testee::behavior_type {
    return {
      [](ok_atom, int x) {
        return x;
      },
      [](ok_atom, double x) {
        return x;
      }
    };
  };
  auto aut = system.spawn(impl);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  using curried_signature = typed_actor<replies_to<int>::with<int>,
                                        replies_to<double>::with<double>>;
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK_NOT_EQUAL(aut.address(), bound.address());
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  static_assert(std::is_same<decltype(bound), curried_signature>::value,
                "bind returned wrong actor handle");
  self->request(bound, infinite, 2.0).receive(
    [](double y) {
      CAF_CHECK_EQUAL(y, 2.0);
    },
    handle_err
  );
  self->request(bound, infinite, 10).receive(
    [](int y) {
      CAF_CHECK_EQUAL(y, 10);
    },
    handle_err
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST(reordering) {
  auto impl = []() -> behavior {
    return {
      [](int x, double y) {
        return x * y;
      }
    };
  };
  auto aut = system.spawn(impl);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  using namespace std::placeholders;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK_NOT_EQUAL(aut, bound);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  self->request(bound, infinite, 2.0, 10).receive(
    [](double y) {
      CAF_CHECK_EQUAL(y, 20.0);
    },
    handle_err
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST(type_safe_reordering) {
  using testee = typed_actor<replies_to<int, double>::with<double>>;
  auto impl = []() -> testee::behavior_type {
    return {
      [](int x, double y) {
        return x * y;
      }
    };
  };
  auto aut = system.spawn(impl);
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  using namespace std::placeholders;
  using swapped_signature = typed_actor<replies_to<double, int>::with<double>>;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK_NOT_EQUAL(aut.address(), bound.address());
  CAF_CHECK_EQUAL(system.registry().running(), 1u);
  static_assert(std::is_same<decltype(bound), swapped_signature>::value,
                "bind returned wrong actor handle");
  self->request(bound, infinite, 2.0, 10).receive(
    [](double y) {
      CAF_CHECK_EQUAL(y, 20.0);
    },
    handle_err
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
