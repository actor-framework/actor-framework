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

TEST("actor_companion forwards messages to a custom handler") {
  auto companion = actor_cast<strong_actor_ptr>(sys.spawn<actor_companion>());
  auto self
    = static_cast<actor_companion*>(actor_cast<abstract_actor*>(companion));
  check_eq(self->enqueue(nullptr, make_message_id(), make_message("42"),
                         nullptr),
           false);
  message msg;
  self->on_enqueue([&msg](mailbox_element_ptr ptr) { msg = ptr->content(); });
  check(self->enqueue(nullptr, make_message_id(), make_message("42"), nullptr));
  check_eq(to_string(msg), R"_(message("42"))_");
  check_eq(self->mailbox().size(), 0u);
}

TEST("actor_companion calls the on_exit handler on shutdown") {
  auto exited = false;
  {
    auto companion = actor_cast<strong_actor_ptr>(sys.spawn<actor_companion>());
    auto self
      = static_cast<actor_companion*>(actor_cast<abstract_actor*>(companion));
    self->on_exit([&exited]() { exited = true; });
  }
  check(exited);
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
