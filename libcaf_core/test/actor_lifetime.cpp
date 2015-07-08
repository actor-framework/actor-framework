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

#define CAF_SUITE actor_lifetime
#include "caf/test/unit_test.hpp"

#include <atomic>

#include "caf/all.hpp"

using namespace caf;

using check_atom = caf::atom_constant<caf::atom("check")>;

namespace {

std::atomic<long> s_testees;
std::atomic<long> s_pending_on_exits;

class testee : public event_based_actor {
public:
  testee();
  ~testee();
  void on_exit() override;
  behavior make_behavior() override;
};

testee::testee() {
  ++s_testees;
  ++s_pending_on_exits;
}

testee::~testee() {
  // avoid weak-vtables warning
  --s_testees;
}

void testee::on_exit() {
  --s_pending_on_exits;
}

behavior testee::make_behavior() {
  return {
    others >> [=] {
      return current_message();
    }
  };
}

template <class ExitMsgType>
behavior tester(event_based_actor* self, const actor& aut) {
  if (std::is_same<ExitMsgType, exit_msg>::value) {
    self->trap_exit(true);
    self->link_to(aut);
  } else {
    self->monitor(aut);
  }
  anon_send_exit(aut, exit_reason::user_shutdown);
  return {
    [self](const ExitMsgType& msg) {
      // must be still alive at this point
      CAF_CHECK_EQUAL(s_testees.load(), 1);
      CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
      CAF_CHECK_EQUAL(self->current_message().vals()->get_reference_count(), 1);
      CAF_CHECK(&msg == self->current_message().at(0));
      // testee might be still running its cleanup code in
      // another worker thread; by waiting some milliseconds, we make sure
      // testee had enough time to return control to the scheduler
      // which in turn destroys it by dropping the last remaining reference
      self->delayed_send(self, std::chrono::milliseconds(30),
                         check_atom::value);
    },
    [self](check_atom) {
      // make sure aut's dtor and on_exit() have been called
      CAF_CHECK_EQUAL(s_testees.load(), 0);
      CAF_CHECK_EQUAL(s_pending_on_exits.load(), 0);
      self->quit();
    }
  };
}

struct fixture {
  ~fixture() {
    await_all_actors_done();
    shutdown();
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(actor_lifetime_tests, fixture)

CAF_TEST(no_spawn_options_and_exit_msg) {
  spawn<no_spawn_options>(tester<exit_msg>, spawn<testee, no_spawn_options>());
}

CAF_TEST(no_spawn_options_and_down_msg) {
  spawn<no_spawn_options>(tester<down_msg>, spawn<testee, no_spawn_options>());
}

CAF_TEST(mixed_spawn_options_and_exit_msg) {
  spawn<detached>(tester<exit_msg>, spawn<testee, no_spawn_options>());
}

CAF_TEST(mixed_spawn_options_and_down_msg) {
  spawn<detached>(tester<down_msg>, spawn<testee, no_spawn_options>());
}

CAF_TEST(mixed_spawn_options2_and_exit_msg) {
  spawn<no_spawn_options>(tester<exit_msg>, spawn<testee, detached>());
}

CAF_TEST(mixed_spawn_options2_and_down_msg) {
  spawn<no_spawn_options>(tester<down_msg>, spawn<testee, detached>());
}

CAF_TEST(detached_and_exit_msg) {
  spawn<detached>(tester<exit_msg>, spawn<testee, detached>());
}

CAF_TEST(detached_and_down_msg) {
  spawn<detached>(tester<down_msg>, spawn<testee, detached>());
}

CAF_TEST_FIXTURE_SCOPE_END()
