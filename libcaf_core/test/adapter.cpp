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

CAF_TEST_FIXTURE_SCOPE(adapter_tests, fixture)

CAF_TEST(identity) {
  auto dbl = system.spawn(testee);
  CAF_CHECK(system.registry().running() == 1);
  auto bound = dbl.bind(1);
  CAF_CHECK(system.registry().running() == 1);
  CAF_CHECK(&bound->home_system() == &dbl->home_system());
  CAF_CHECK(bound->node() == dbl->node());
  CAF_CHECK(bound != dbl);
  CAF_CHECK(bound->id() != dbl->id());
  anon_send_exit(bound, exit_reason::kill);
  anon_send_exit(dbl, exit_reason::kill);
}

// bound actor spawned dead if decorated
// actor is already dead upon spawning
CAF_TEST(lifetime_1) {
  auto dbl = system.spawn(testee);
  self->monitor(dbl);
  anon_send_exit(dbl, exit_reason::kill);
  wait_until_exited();
  auto bound = dbl.bind(1);
  CAF_CHECK(exited(bound));
}

// bound actor exits when decorated actor exits
CAF_TEST(lifetime_2) {
  auto dbl = system.spawn(testee);
  auto bound = dbl.bind(1);
  self->monitor(bound);
  anon_send(dbl, message{});
  wait_until_exited();
}

// 1) ignores down message not from the decorated actor
// 2) exits by receiving an exit message
// 3) exit has no effect on decorated actor
CAF_TEST(lifetime_3) {
  auto dbl = system.spawn(testee);
  auto bound = dbl.bind(1);
  anon_send(bound, down_msg{self->address(),
                             exit_reason::kill});
  CAF_CHECK(! exited(bound));
  self->monitor(bound);
  auto em_sender = system.spawn(testee);
  em_sender->link_to(bound->address());
  anon_send_exit(em_sender, exit_reason::kill);
  wait_until_exited();
  self->request(dbl, 1).receive(
    [](int v) {
      CAF_CHECK(v == 2);
    },
    [](error) {
      CAF_CHECK(false);
    }
  );
  anon_send_exit(dbl, exit_reason::kill);
}

CAF_TEST(request_response_promise) {
  auto dbl = system.spawn(testee);
  auto bound = dbl.bind(1);
  anon_send_exit(bound, exit_reason::kill);
  CAF_CHECK(exited(bound));
  self->request(bound, message{}).receive(
    [](int) {
      CAF_CHECK(false);
    },
    [](error err) {
      CAF_CHECK(err.code() == static_cast<uint8_t>(sec::request_receiver_down));
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
  CAF_CHECK(system.registry().running() == 1);
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK(aut.id() != bound.id());
  CAF_CHECK(aut.node() == bound.node());
  CAF_CHECK(aut != bound);
  CAF_CHECK(system.registry().running() == 1);
  self->request(bound, 2.0).receive(
    [](double y) {
      CAF_CHECK(y == 2.0);
    }
  );
  self->request(bound, 10).receive(
    [](int y) {
      CAF_CHECK(y == 10);
    }
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST(full_currying) {
  auto dbl_actor = system.spawn(testee);
  auto bound = dbl_actor.bind(1);
  self->request(bound, message{}).receive(
    [](int v) {
      CAF_CHECK(v == 2);
    },
    [](error) {
      CAF_CHECK(false);
    }
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
  CAF_CHECK(system.registry().running() == 1);
  using curried_signature = typed_actor<replies_to<int>::with<int>,
                                        replies_to<double>::with<double>>;
  auto bound = aut.bind(ok_atom::value, _1);
  CAF_CHECK(aut != bound);
  CAF_CHECK(system.registry().running() == 1);
  static_assert(std::is_same<decltype(bound), curried_signature>::value,
                "bind returned wrong actor handle");
  self->request(bound, 2.0).receive(
    [](double y) {
      CAF_CHECK(y == 2.0);
    }
  );
  self->request(bound, 10).receive(
    [](int y) {
      CAF_CHECK(y == 10);
    }
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
  CAF_CHECK(system.registry().running() == 1);
  using namespace std::placeholders;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK(aut != bound);
  CAF_CHECK(system.registry().running() == 1);
  self->request(bound, 2.0, 10).receive(
    [](double y) {
      CAF_CHECK(y == 20.0);
    }
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
  CAF_CHECK(system.registry().running() == 1);
  using namespace std::placeholders;
  using swapped_signature = typed_actor<replies_to<double, int>::with<double>>;
  auto bound = aut.bind(_2, _1);
  CAF_CHECK(aut != bound);
  CAF_CHECK(system.registry().running() == 1);
  static_assert(std::is_same<decltype(bound), swapped_signature>::value,
                "bind returned wrong actor handle");
  self->request(bound, 2.0, 10).receive(
    [](double y) {
      CAF_CHECK(y == 20.0);
    }
  );
  self->send_exit(aut, exit_reason::kill);
}

CAF_TEST_FIXTURE_SCOPE_END()
