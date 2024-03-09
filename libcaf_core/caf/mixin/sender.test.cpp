// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/mixin/sender.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"

#include <chrono>

using namespace caf;

using std::chrono::seconds;

namespace {

behavior testee_impl(event_based_actor* self) {
  self->set_default_handler(reflect);
  return {
    [] {
      // nop
    },
  };
}

struct fixture : test::fixture::deterministic {
  actor testee;
  scoped_actor self{sys};

  std::string hello = "hello world";

  fixture() {
    testee = sys.spawn(testee_impl);
  }

  ~fixture() {
    anon_send_exit(testee, exit_reason::user_shutdown);
  }
};

WITH_FIXTURE(fixture) {

TEST("delayed actor messages receive responses") {
  self->delayed_send(testee, seconds(1), hello);
  trigger_timeout();
  expect<std::string>().with(hello).from(self).to(testee);
  // expect<std::string>().with(hello).from(testee).to(self);
  auto received = std::make_shared<bool>(false);
  self->receive([this, received](std::string received_str) {
    *received = true;
    check_eq(received_str, hello);
  });
  check(*received);
  self->scheduled_send(testee, self->clock().now() + seconds(1), hello);
  trigger_timeout();
  expect<std::string>().with(hello).from(self).to(testee);
  *received = false;
  self->receive([this, received](std::string received_str) {
    *received = true;
    check_eq(received_str, hello);
  });
  check(*received);
}

TEST("anonymous messages receive no response") {
  self->anon_send(testee, hello);
  expect<std::string>().with(hello).to(testee);
  check(!allow<std::string>().with(hello).from(testee).to(self));
  self->delayed_anon_send(testee, seconds(1), hello);
  trigger_timeout();
  expect<std::string>().with(hello).to(testee);
  check(!allow<std::string>().with(hello).from(testee).to(self));
  self->scheduled_anon_send(testee, self->clock().now() + seconds(1), hello);
  trigger_timeout();
  expect<std::string>().with(hello).to(testee);
  check(!allow<std::string>().with(hello).from(testee).to(self));
}

#ifdef CAF_ENABLE_EXCEPTIONS

TEST("exceptions while processing a message will send error to the sender") {
  auto worker = sys.spawn([] {
    return behavior{
      [](int) { throw std::runtime_error(""); },
    };
  });
  dispatch_messages();
  auto client = sys.spawn([worker](event_based_actor*) -> behavior {
    return [](message) { //
      test::runnable::current().fail("unexpected handler called");
    };
  });
  inject().with(42).from(client).to(worker);
  expect<error>().with(sec::runtime_error).from(worker).to(client);
}

#endif // CAF_ENABLE_EXCEPTIONS

} // WITH_FIXTURE(fixture)

} // namespace
