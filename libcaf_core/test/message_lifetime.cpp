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

  ~testee() override {
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
  actor_system_config cfg;
  actor_system system{cfg};
};

struct fail_on_copy {
  int value;

  fail_on_copy(int x = 0) : value(x) {
    // nop
  }

  fail_on_copy(fail_on_copy&&) = default;

  fail_on_copy& operator=(fail_on_copy&&) = default;

  fail_on_copy(const fail_on_copy&) {
    CAF_FAIL("fail_on_copy: copy constructor called");
  }

  fail_on_copy& operator=(const fail_on_copy&) {
    CAF_FAIL("fail_on_copy: copy assign operator called");
  }
};

template <class Inspector>
typename Inspector::result_type inspect(Inspector& f, fail_on_copy& x) {
  return f(x.value);
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(message_lifetime_tests, fixture)

CAF_TEST(nocopy_in_scoped_actor) {
  auto msg = make_message(fail_on_copy{1});
  scoped_actor self{system};
  self->send(self, msg);
  self->receive(
    [&](const fail_on_copy& x) {
      CAF_CHECK_EQUAL(x.value, 1);
      CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 2u);
    }
  );
  CAF_CHECK_EQUAL(msg.cvals()->get_reference_count(), 1u);
}

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

CAF_TEST(message_lifetime_in_spawned_actor) {
  for (size_t i = 0; i < 100; ++i)
    system.spawn<tester>(system.spawn<testee>());
}

CAF_TEST_FIXTURE_SCOPE_END()
