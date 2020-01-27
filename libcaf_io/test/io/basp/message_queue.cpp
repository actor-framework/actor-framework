/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2019 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#define CAF_SUITE io.basp.message_queue

#include "caf/io/basp/message_queue.hpp"

#include "caf/test/dsl.hpp"

#include "caf/actor_cast.hpp"
#include "caf/actor_system.hpp"
#include "caf/behavior.hpp"

using namespace caf;

namespace {

behavior testee_impl() {
  return {[](ok_atom, int) {
    // nop
  }};
}

struct fixture : test_coordinator_fixture<> {
  io::basp::message_queue queue;
  strong_actor_ptr testee;

  fixture() {
    auto hdl = sys.spawn<lazy_init>(testee_impl);
    testee = actor_cast<strong_actor_ptr>(hdl);
  }

  void acquire_ids(size_t num) {
    for (size_t i = 0; i < num; ++i)
      queue.new_id();
  }

  void push(int msg_id) {
    queue.push(nullptr, static_cast<uint64_t>(msg_id), testee,
               make_mailbox_element(self->ctrl(), make_message_id(), {},
                                    ok_atom_v, msg_id));
  }
};

} // namespace

CAF_TEST_FIXTURE_SCOPE(message_queue_tests, fixture)

CAF_TEST(default construction) {
  CAF_CHECK_EQUAL(queue.next_id, 0u);
  CAF_CHECK_EQUAL(queue.next_undelivered, 0u);
  CAF_CHECK_EQUAL(queue.pending.size(), 0u);
}

CAF_TEST(ascending IDs) {
  CAF_CHECK_EQUAL(queue.new_id(), 0u);
  CAF_CHECK_EQUAL(queue.new_id(), 1u);
  CAF_CHECK_EQUAL(queue.new_id(), 2u);
  CAF_CHECK_EQUAL(queue.next_undelivered, 0u);
}

CAF_TEST(push order 0 - 1 - 2) {
  acquire_ids(3);
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  push(1);
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  push(2);
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(push order 0 - 2 - 1) {
  acquire_ids(3);
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  push(2);
  disallow((ok_atom, int), from(self).to(testee));
  push(1);
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(push order 1 - 0 - 2) {
  acquire_ids(3);
  push(1);
  disallow((ok_atom, int), from(self).to(testee));
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  push(2);
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(push order 1 - 2 - 0) {
  acquire_ids(3);
  push(1);
  disallow((ok_atom, int), from(self).to(testee));
  push(2);
  disallow((ok_atom, int), from(self).to(testee));
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(push order 2 - 0 - 1) {
  acquire_ids(3);
  push(2);
  disallow((ok_atom, int), from(self).to(testee));
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  push(1);
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(push order 2 - 1 - 0) {
  acquire_ids(3);
  push(2);
  disallow((ok_atom, int), from(self).to(testee));
  push(1);
  disallow((ok_atom, int), from(self).to(testee));
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  expect((ok_atom, int), from(self).to(testee).with(_, 1));
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST(dropping) {
  acquire_ids(3);
  push(2);
  disallow((ok_atom, int), from(self).to(testee));
  queue.drop(nullptr, 1);
  disallow((ok_atom, int), from(self).to(testee));
  push(0);
  expect((ok_atom, int), from(self).to(testee).with(_, 0));
  expect((ok_atom, int), from(self).to(testee).with(_, 2));
}

CAF_TEST_FIXTURE_SCOPE_END()
