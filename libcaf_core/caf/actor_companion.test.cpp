// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/actor_companion.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/mailbox_element.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("actor_companion forwards messages to a custom handler") {
  auto uut = sys.make_companion();
  check_eq(uut->enqueue(make_mailbox_element(nullptr, make_message_id(), "42"),
                        nullptr),
           false);
  message msg;
  uut->on_enqueue([&msg](mailbox_element_ptr ptr) { msg = ptr->content(); });
  check(uut->enqueue(make_mailbox_element(nullptr, make_message_id(), "42"),
                     nullptr));
  check_eq(to_string(msg), R"_(message("42"))_");
}

TEST("actor_companion calls the on_exit handler on shutdown") {
  auto exited = std::make_shared<bool>(false);
  {
    auto uut = sys.make_companion();
    uut->on_exit([exited]() { *exited = true; });
  }
  check(*exited);
}

TEST("actor_companion can send messages") {
  auto adder = sys.spawn([] {
    return behavior{
      [](int value) { return value + 1; },
    };
  });
  auto uut = sys.make_companion();
  auto msg = std::make_shared<message>();
  uut->on_enqueue([msg](mailbox_element_ptr ptr) { *msg = ptr->content(); });
  uut->mail(41).send(adder);
  expect<int>().with(41).to(adder);
  if (check_ne(msg, nullptr)) {
    check_eq(to_string(*msg), R"_(message(42))_");
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
