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

#define CAF_SUITE actor_clock
#include "caf/test/dsl.hpp"

#include <chrono>
#include <memory>

#include "caf/all.hpp"
#include "caf/detail/test_actor_clock.hpp"
#include "caf/raw_event_based_actor.hpp"

using namespace caf;

namespace {

using std::chrono::seconds;

struct testee_state {
  uint64_t timeout_id = 41;
};

behavior testee(stateful_actor<testee_state, raw_event_based_actor>* self,
                detail::test_actor_clock* t) {
  return {
    [=](ok_atom) {
      auto n = t->now() + seconds(10);
      self->state.timeout_id += 1;
      t->set_ordinary_timeout(n, self, atom(""), self->state.timeout_id);
    },
    [=](add_atom) {
      auto n = t->now() + seconds(10);
      self->state.timeout_id += 1;
      t->set_multi_timeout(n, self, atom(""), self->state.timeout_id);
    },
    [=](put_atom) {
      auto n = t->now() + seconds(10);
      self->state.timeout_id += 1;
      auto mid = make_message_id(self->state.timeout_id).response_id();
      t->set_request_timeout(n, self, mid);
    },
    [](const timeout_msg&) {
      // nop
    },
    [](const error&) {
      // nop
    },
    [](const std::string&) {
      // nop
    },
    [=](group& grp) {
      self->join(grp);
    },
    [=](exit_msg& x) {
      self->quit(x.reason);
    }
  };
}

struct fixture : test_coordinator_fixture<> {
  detail::test_actor_clock t;
  actor aut;

  fixture() : aut(sys.spawn<lazy_init>(testee, &t)) {
    // nop
  }
};

struct tid {
  uint32_t value;
};

inline bool operator==(const timeout_msg& x, const tid& y) {
  return x.timeout_id == y.value;
}

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(timer_tests, fixture)

CAF_TEST(single_receive_timeout) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom::value);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
}

CAF_TEST(override_receive_timeout) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom::value);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT call t.set_timeout() again.
  self->send(aut, ok_atom::value);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));
}

CAF_TEST(multi_timeout) {
  // Have AUT call t.set_multi_timeout().
  self->send(aut, add_atom::value);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time just a little bit.
  t.advance_time(seconds(5));
  // Have AUT call t.set_multi_timeout() again.
  self->send(aut, add_atom::value);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send timeout message.
  t.advance_time(seconds(5));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send second timeout message.
  t.advance_time(seconds(5));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));
}

CAF_TEST(mixed_receive_and_multi_timeouts) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, add_atom::value);
  expect((add_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time just a little bit.
  t.advance_time(seconds(5));
  // Have AUT call t.set_multi_timeout() again.
  self->send(aut, ok_atom::value);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send timeout message.
  t.advance_time(seconds(5));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send second timeout message.
  t.advance_time(seconds(5));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{43}));

}

CAF_TEST(single_request_timeout) {
  // Have AUT call t.set_request_timeout().
  self->send(aut, put_atom::value);
  expect((put_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Advance time to send timeout message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((error), from(aut).to(aut).with(sec::request_timeout));
}

CAF_TEST(mixed_receive_and_request_timeouts) {
  // Have AUT call t.set_receive_timeout().
  self->send(aut, ok_atom::value);
  expect((ok_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Cause the request timeout to arrive later.
  t.advance_time(seconds(5));
  // Have AUT call t.set_request_timeout().
  self->send(aut, put_atom::value);
  expect((put_atom), from(self).to(aut).with(_));
  CAF_CHECK_EQUAL(t.schedule().size(), 2u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 2u);
  // Advance time to send receive timeout message.
  t.advance_time(seconds(5));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 1u);
  // Have AUT receive the timeout.
  expect((timeout_msg), from(aut).to(aut).with(tid{42}));
  // Advance time to send request timeout message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the timeout.
  expect((error), from(aut).to(aut).with(sec::request_timeout));
}

CAF_TEST(delay_actor_message) {
  // Schedule a message for now + 10s.
  auto n = t.now() + seconds(10);
  auto autptr = actor_cast<strong_actor_ptr>(aut);
  t.schedule_message(n, autptr,
                     make_mailbox_element(autptr, make_message_id(),
                                          no_stages, "foo"));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Advance time to send the message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the message.
  expect((std::string), from(aut).to(aut).with("foo"));
}

CAF_TEST(delay_group_message) {
  // Have AUT join the group.
  auto grp = sys.groups().anonymous();
  self->send(aut, grp);
  expect((group), from(self).to(aut).with(_));
  // Schedule a message for now + 10s.
  auto n = t.now() + seconds(10);
  auto autptr = actor_cast<strong_actor_ptr>(aut);
  t.schedule_message(n, std::move(grp), autptr, make_message("foo"));
  CAF_CHECK_EQUAL(t.schedule().size(), 1u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Advance time to send the message.
  t.advance_time(seconds(10));
  CAF_CHECK_EQUAL(t.schedule().size(), 0u);
  CAF_CHECK_EQUAL(t.actor_lookup().size(), 0u);
  // Have AUT receive the message.
  expect((std::string), from(aut).to(aut).with("foo"));
  // Kill AUT (necessary because the group keeps a reference around).
  self->send_exit(aut, exit_reason::kill);
  expect((exit_msg), from(self).to(aut).with(_));
}

CAF_TEST_FIXTURE_SCOPE_END()
