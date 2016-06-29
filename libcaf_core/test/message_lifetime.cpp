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

#define CAF_SUITE message_lifetime
#include "caf/test/unit_test.hpp"

#include <atomic>
#include <iostream>

#include "caf/all.hpp"

using namespace caf;

namespace {

class testee : public event_based_actor {
public:
  testee(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  ~testee() {
    // nop
  }

  behavior make_behavior() override {
    // reflecting a message increases its reference count by one
    set_default_handler(reflect_and_quit);
    return {
      [] {
        // nop
      }
    };
  }
};

class tester : public event_based_actor {
public:
  tester(actor_config& cfg, actor aut)
      : event_based_actor(cfg),
        aut_(std::move(aut)),
        msg_(make_message(1, 2, 3)) {
    set_down_handler([=](down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
      CAF_CHECK_EQUAL(dm.source, aut_.address());
      quit();
    });
  }

  behavior make_behavior() override {
    monitor(aut_);
    send(aut_, msg_);
    return {
      [=](int a, int b, int c) {
        CAF_CHECK_EQUAL(a, 1);
        CAF_CHECK_EQUAL(b, 2);
        CAF_CHECK_EQUAL(c, 3);
      }
    };
  }

private:
  actor aut_;
  message msg_;
};

struct fixture {
  fixture() : system(cfg) {
    // nop
  }

  template <spawn_options Os>
  void test_message_lifetime() {
    // put some preassure on the scheduler (check for thread safety)
    for (size_t i = 0; i < 100; ++i)
      system.spawn<tester>(system.spawn<testee, Os>());
  }

  actor_system_config cfg;
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(message_lifetime_tests, fixture)

CAF_TEST(message_lifetime_in_scoped_actor) {
  auto msg = make_message(1, 2, 3);
  scoped_actor self{system};
  self->send(self, msg);
  self->receive(
    [&](int a, int b, int c) {
      CAF_CHECK_EQUAL(a, 1);
      CAF_CHECK_EQUAL(b, 2);
      CAF_CHECK_EQUAL(c, 3);
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 2u);
    }
  );
  CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1u);
  msg = make_message(42);
  self->send(self, msg);
  CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 2u);
  self->receive(
    [&](int& value) {
      CAF_CHECK_NOT_EQUAL(&value, msg.at(0));
      value = 10;
    }
  );
  CAF_CHECK_EQUAL(msg.get_as<int>(0), 42);
}

CAF_TEST(message_lifetime_no_spawn_options) {
  test_message_lifetime<no_spawn_options>();
}

CAF_TEST(message_lifetime_priority_aware) {
  test_message_lifetime<priority_aware>();
}

CAF_TEST_FIXTURE_SCOPE_END()
