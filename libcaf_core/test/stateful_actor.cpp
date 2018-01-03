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

#define CAF_SUITE stateful_actor
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

using namespace std;
using namespace caf;

namespace {

using typed_adder_actor = typed_actor<reacts_to<add_atom, int>,
                                      replies_to<get_atom>::with<int>>;

struct counter {
  int value = 0;
  local_actor* self_;
};

behavior adder(stateful_actor<counter>* self) {
  return {
    [=](add_atom, int x) {
      self->state.value += x;
    },
    [=](get_atom) {
      return self->state.value;
    }
  };
}

class adder_class : public stateful_actor<counter> {
public:
  adder_class(actor_config& cfg) : stateful_actor<counter>(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return adder(this);
  }
};

typed_adder_actor::behavior_type
typed_adder(typed_adder_actor::stateful_pointer<counter> self) {
  return {
    [=](add_atom, int x) {
      self->state.value += x;
    },
    [=](get_atom) {
      return self->state.value;
    }
  };
}

class typed_adder_class : public typed_adder_actor::stateful_base<counter> {
public:
  using super = typed_adder_actor::stateful_base<counter>;

  typed_adder_class(actor_config& cfg) : super(cfg) {
    // nop
  }

  behavior_type make_behavior() override {
    return typed_adder(this);
  }
};

struct fixture {
  actor_system_config cfg;
  actor_system system;

  fixture() : system(cfg) {
    // nop
  }

  template <class ActorUnderTest>
  void test_adder(ActorUnderTest aut) {
    scoped_actor self{system};
    self->send(aut, add_atom::value, 7);
    self->send(aut, add_atom::value, 4);
    self->send(aut, add_atom::value, 9);
    self->request(aut, infinite, get_atom::value).receive(
      [](int x) {
        CAF_CHECK_EQUAL(x, 20);
      },
      ERROR_HANDLER
    );
  }

  template <class State>
  void test_name(const char* expected) {
    auto aut = system.spawn([](stateful_actor<State>* self) -> behavior {
      return [=](get_atom) {
        self->quit();
        return self->name();
      };
    });
    scoped_actor self{system};
    self->request(aut, infinite, get_atom::value).receive(
      [&](const string& str) {
        CAF_CHECK_EQUAL(str, expected);
      },
      ERROR_HANDLER
    );
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(dynamic_stateful_actor_tests, fixture)

CAF_TEST(dynamic_stateful_actor) {
  CAF_REQUIRE(monitored + monitored == monitored);
  test_adder(system.spawn(adder));
}

CAF_TEST(typed_stateful_actor) {
  test_adder(system.spawn(typed_adder));
}

CAF_TEST(dynamic_stateful_actor_class) {
  test_adder(system.spawn<adder_class>());
}

CAF_TEST(typed_stateful_actor_class) {
  test_adder(system.spawn<typed_adder_class>());
}

CAF_TEST(no_name) {
  struct state {
    // empty
  };
  test_name<state>("scheduled_actor");
}

CAF_TEST(char_name) {
  struct state {
    const char* name = "testee";
  };
  test_name<state>("testee");
}

CAF_TEST(string_name) {
  struct state {
    string name = "testee2";
  };
  test_name<state>("testee2");
}

CAF_TEST_FIXTURE_SCOPE_END()
