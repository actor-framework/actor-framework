// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/io/basp/message_queue.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/message_id.hpp"

using namespace caf;

namespace {

behavior snk_impl() {
  return {
    [](ok_atom, int) {
      // nop
    },
  };
}

struct fixture : test::fixture::deterministic {
  actor src;
  actor snk;
  io::basp::message_queue queue;

  fixture() {
    src = sys.spawn(snk_impl);
    snk = sys.spawn(snk_impl);
  }

  void acquire_ids(size_t num) {
    for (size_t i = 0; i < num; ++i)
      queue.new_id();
  }

  void push(int msg_id) {
    queue.push(nullptr, static_cast<uint64_t>(msg_id),
               actor_cast<strong_actor_ptr>(snk),
               make_mailbox_element(actor_cast<strong_actor_ptr>(src),
                                    make_message_id(), ok_atom_v, msg_id));
  }
};

WITH_FIXTURE(fixture) {

TEST("default construction") {
  check_eq(queue.next_id, 0u);
  check_eq(queue.next_undelivered, 0u);
  check_eq(queue.pending.size(), 0u);
}

TEST("ascending IDs") {
  check_eq(queue.new_id(), 0u);
  check_eq(queue.new_id(), 1u);
  check_eq(queue.new_id(), 2u);
  check_eq(queue.next_undelivered, 0u);
}

TEST("push order 0 - 1 - 2") {
  acquire_ids(3);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  push(1);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  push(2);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("push order 0 - 2 - 1") {
  acquire_ids(3);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  push(2);
  disallow<ok_atom, int>().from(src).to(snk);
  push(1);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("push order 1 - 0 - 2") {
  acquire_ids(3);
  push(1);
  disallow<ok_atom, int>().from(src).to(snk);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  push(2);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("push order 1 - 2 - 0") {
  acquire_ids(3);
  push(1);
  disallow<ok_atom, int>().from(src).to(snk);
  push(2);
  disallow<ok_atom, int>().from(src).to(snk);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("push order 2 - 0 - 1") {
  acquire_ids(3);
  push(2);
  disallow<ok_atom, int>().from(src).to(snk);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  push(1);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("push order 2 - 1 - 0") {
  acquire_ids(3);
  push(2);
  disallow<ok_atom, int>().from(src).to(snk);
  push(1);
  disallow<ok_atom, int>().from(src).to(snk);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 1).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

TEST("dropping") {
  acquire_ids(3);
  push(2);
  disallow<ok_atom, int>().from(src).to(snk);
  queue.drop(nullptr, 1);
  disallow<ok_atom, int>().from(src).to(snk);
  push(0);
  expect<ok_atom, int>().with(std::ignore, 0).from(src).to(snk);
  expect<ok_atom, int>().with(std::ignore, 2).from(src).to(snk);
}

} // WITH_FIXTURE(fixture)

} // namespace
