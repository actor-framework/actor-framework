// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/sec.hpp"

using namespace caf;

namespace {

WITH_FIXTURE(test::fixture::deterministic) {

TEST("delegation moves responsibility for a request to another actor") {
  auto count = std::make_shared<int32_t>(0);
  auto worker = sys.spawn([count] {
    return behavior{
      [count](int32_t value) {
        *count += value;
        return *count;
      },
    };
  });
  auto delegator = sys.spawn([worker](event_based_actor* self) {
    return behavior{
      [self, worker](int32_t x) { return self->delegate(worker, x); },
      [self, worker](std::string& x) {
        return self->delegate(worker, std::move(x));
      },
    };
  });
  SECTION("the delegatee responds to the original sender") {
    auto client = sys.spawn([delegator](event_based_actor* self) {
      self->mail(2).send(delegator);
      self->mail(3).send(delegator);
      return behavior{
        [](int32_t) {},
      };
    });
    expect<int32_t>().with(2).from(client).to(delegator);
    expect<int32_t>().with(2).from(client).to(worker);
    expect<int32_t>().with(2).from(worker).to(client);
    check_eq(*count, 2);
    expect<int32_t>().with(3).from(client).to(delegator);
    expect<int32_t>().with(3).from(client).to(worker);
    expect<int32_t>().with(5).from(worker).to(client);
    check_eq(*count, 5);
  }
  SECTION("the delegatee sends errors to the original sender") {
    auto client = sys.spawn([delegator](event_based_actor* self) {
      self->mail("foo").send(delegator);
      return behavior{
        [](int32_t) {},
      };
    });
    auto client_err = std::make_shared<error>();
    auto observer = sys.spawn([client, client_err](event_based_actor* self) {
      self->monitor(client, [client_err](const error& reason) {
        *client_err = reason;
      });
      return behavior{
        [](int32_t) {},
      };
    });
    expect<std::string>().with("foo").from(client).to(delegator);
    expect<std::string>().with("foo").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
    expect<action>().to(observer);
    check_eq(*client_err, sec::unexpected_message);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
