// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/sec.hpp"

using namespace caf;

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
      self->send(delegator, 2);
      self->send(delegator, 3);
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
      self->send(delegator, "foo");
      return behavior{
        [](int32_t) {},
      };
    });
    auto observer = sys.spawn([client](event_based_actor* self) {
      self->monitor(client);
      self->set_down_handler([](const down_msg&) {});
      return behavior{
        [](int32_t) {},
      };
    });
    expect<std::string>().with("foo").from(client).to(delegator);
    expect<std::string>().with("foo").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
    expect<down_msg>()
      .with(down_msg{client.address(), sec::unexpected_message})
      .from(client)
      .to(observer);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
