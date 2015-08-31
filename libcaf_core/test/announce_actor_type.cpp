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

#define CAF_SUITE announce_actor_type
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

#include "caf/experimental/announce_actor_type.hpp"

#include "caf/detail/actor_registry.hpp"

using namespace caf;
using namespace caf::experimental;

using std::endl;

namespace {

struct fixture {
  actor aut;
  actor spawner;

  fixture() {
    auto registry = detail::singletons::get_actor_registry();
    spawner = registry->get_named(atom("SpawnServ"));
  }

  void set_aut(message args, bool expect_fail = false) {
    CAF_MESSAGE("set aut");
    scoped_actor self;
    self->on_sync_failure([&] {
      CAF_TEST_ERROR("received unexpeced sync. response: "
                     << to_string(self->current_message()));
    });
    if (expect_fail) {
      self->sync_send(spawner, get_atom::value, "test_actor", std::move(args)).await(
        [&](error_atom, const std::string&) {
          CAF_MESSAGE("received error_atom (expected)");
        }
      );
    } else {
      self->sync_send(spawner, get_atom::value, "test_actor", std::move(args)).await(
        [&](ok_atom, actor_addr res, const std::set<std::string>& ifs) {
          CAF_REQUIRE(res != invalid_actor_addr);
          aut = actor_cast<actor>(res);
          CAF_CHECK(ifs.empty());
        }
      );
    }
  }

  ~fixture() {
    if (aut != invalid_actor) {
      scoped_actor self;
      self->monitor(aut);
      self->receive(
        [](const down_msg& dm) {
          CAF_CHECK(dm.reason == exit_reason::normal);
        }
      );
    }
    await_all_actors_done();
    shutdown();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(announce_actor_type_tests, fixture)

CAF_TEST(fun_no_args) {
  auto test_actor = [] {
    CAF_MESSAGE("inside test_actor");
  };
  announce_actor_type("test_actor", test_actor);
  set_aut(make_message());
}

CAF_TEST(fun_no_args_selfptr) {
  auto test_actor = [](event_based_actor*) {
    CAF_MESSAGE("inside test_actor");
  };
  announce_actor_type("test_actor", test_actor);
  set_aut(make_message());
}
CAF_TEST(fun_one_arg) {
  auto test_actor = [](int i) {
    CAF_CHECK_EQUAL(i, 42);
  };
  announce_actor_type("test_actor", test_actor);
  set_aut(make_message(42));
}

CAF_TEST(fun_one_arg_selfptr) {
  auto test_actor = [](event_based_actor*, int i) {
    CAF_CHECK_EQUAL(i, 42);
  };
  announce_actor_type("test_actor", test_actor);
  set_aut(make_message(42));
}

CAF_TEST(class_no_arg) {
  struct test_actor : event_based_actor {
    behavior make_behavior() override {
      return {};
    }
  };
  announce_actor_type<test_actor>("test_actor");
  set_aut(make_message(42), true);
  set_aut(make_message());
}

CAF_TEST(class_one_arg) {
  struct test_actor : event_based_actor {
    test_actor(int value) {
      CAF_CHECK_EQUAL(value, 42);
    }
    behavior make_behavior() override {
      return {};
    }
  };
  announce_actor_type<test_actor, const int&>("test_actor");
  set_aut(make_message(), true);
  set_aut(make_message(42));
}

CAF_TEST_FIXTURE_SCOPE_END()
