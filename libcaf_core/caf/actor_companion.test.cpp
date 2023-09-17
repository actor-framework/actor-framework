// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/actor_companion.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/mailbox_element.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("actor_companion can enqueue messages with call to on_enqueue handler") {
  auto companion = actor_cast<strong_actor_ptr>(sys.spawn<actor_companion>());
  auto self
    = static_cast<actor_companion*>(actor_cast<abstract_actor*>(companion));
  check_eq(self->enqueue(nullptr, make_message_id(), make_message("42"),
                         nullptr),
           false);
  self->on_enqueue([=](mailbox_element_ptr ptr) {
    check_eq(to_string(ptr->content()), "message(\"42\")");
    check_eq(self->mailbox().size(), 0u);
  });
  SECTION("actor_companion can enqueue from mailbox_element_ptr") {
    check(self->enqueue(make_mailbox_element(companion, make_message_id(), {},
                                             make_message("42")),
                        nullptr));
  }
  SECTION("actor_companion can enqueue from message_id and caf::message") {
    check(
      self->enqueue(nullptr, make_message_id(), make_message("42"), nullptr));
  }
}

TEST("actor_companion can exit with call to on_exit handler") {
  auto companion = actor_cast<strong_actor_ptr>(sys.spawn<actor_companion>());
  auto self
    = static_cast<actor_companion*>(actor_cast<abstract_actor*>(companion));
  auto exit_flag = false;
  self->on_exit([&exit_flag]() { exit_flag = true; });
  self->on_exit();
  check(exit_flag);
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
