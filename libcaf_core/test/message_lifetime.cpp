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
  testee();
  ~testee();
  behavior make_behavior() override;
};

testee::testee() {
  // nop
}

testee::~testee() {
  // nop
}

behavior testee::make_behavior() {
  return {
    others >> [=] {
      CAF_CHECK_EQUAL(current_message().cvals()->get_reference_count(), 2);
      quit();
      return std::move(current_message());
    }
  };
}

class tester : public event_based_actor {
public:
  explicit tester(actor aut)
      : aut_(std::move(aut)),
        msg_(make_message(1, 2, 3)) {
    // nop
  }

  behavior make_behavior() override;
private:
  actor aut_;
  message msg_;
};

behavior tester::make_behavior() {
  monitor(aut_);
  send(aut_, msg_);
  return {
    [=](int a, int b, int c) {
      CAF_CHECK_EQUAL(a, 1);
      CAF_CHECK_EQUAL(b, 2);
      CAF_CHECK_EQUAL(c, 3);
      CAF_CHECK(current_message().cvals()->get_reference_count() == 2
                && current_message().cvals().get() == msg_.cvals().get());
    },
    [=](const down_msg& dm) {
      CAF_CHECK(dm.source == aut_
                && dm.reason == exit_reason::normal
                && current_message().cvals()->get_reference_count() == 1);
      quit();
    },
    others >> [&] {
      CAF_TEST_ERROR("Unexpected message: " << to_string(current_message()));
    }
  };
}

template <spawn_options Os>
void test_message_lifetime() {
  // put some preassure on the scheduler (check for thread safety)
  for (size_t i = 0; i < 100; ++i) {
    spawn<tester>(spawn<testee, Os>());
  }
}

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(message_lifetime_tests, fixture)

CAF_TEST(message_lifetime_in_scoped_actor) {
  auto msg = make_message(1, 2, 3);
  scoped_actor self;
  self->send(self, msg);
  self->receive(
    [&](int a, int b, int c) {
      CAF_CHECK_EQUAL(a, 1);
      CAF_CHECK_EQUAL(b, 2);
      CAF_CHECK_EQUAL(c, 3);
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 2);
      CAF_CHECK_EQUAL(self->current_message().cvals()->get_reference_count(), 2);
      CAF_CHECK(self->current_message().cvals().get() == msg.cvals().get());
    }
  );
  CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1);
  msg = make_message(42);
  self->send(self, msg);
  self->receive(
    [&](int& value) {
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1);
      CAF_CHECK_EQUAL(self->current_message().cvals()->get_reference_count(), 1);
      CAF_CHECK(self->current_message().cvals().get() != msg.cvals().get());
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
