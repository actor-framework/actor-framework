// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/response_handle.hpp"
#include "caf/scoped_actor.hpp"

using namespace caf;
using namespace std::literals;

namespace {

message_handler handle_a() {
  return {
    [](int) { return "a"; },
  };
}

message_handler handle_b() {
  return {
    [](int) { return "b"; },
  };
}

WITH_FIXTURE(test::fixture::deterministic) {

TEST("message handler may be assigned") {
  scoped_actor self{sys};
  auto received = std::make_shared<bool>(false);
  auto handler = handle_a();
  handler.assign(handle_b());
  auto testee = sys.spawn([=]() { return handler; });
  self->mail(int{1}).send(testee);
  dispatch_messages();
  self->receive([this, received](const std::string& str) {
    *received = true;
    check_eq(str, "b");
  });
  self->send_exit(testee, exit_reason::user_shutdown);
  dispatch_messages();
  require(*received);
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
