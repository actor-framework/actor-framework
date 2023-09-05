// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/sec.hpp"
#include "caf/send.hpp"

namespace fixture = caf::test::fixture;

WITH_FIXTURE(fixture::deterministic) {

#ifdef CAF_ENABLE_EXCEPTIONS
TEST("delegates pass on the response properly to the sender") {
  using namespace caf;
  auto count = std::make_shared<int32_t>(0);
  auto worker = sys.spawn([count] {
    return behavior{
      [count](int32_t value) {
        *count = *count + value;
        return *count;
      },
    };
  });
  auto delegator = sys.spawn(
    [](event_based_actor* self, caf::actor worker) {
      return behavior{
        [self, worker](int32_t x) {
          auto promise = self->make_response_promise<int>();
          return promise.delegate(worker, x);
        },
        [self, worker](std::string x) {
          auto promise = self->make_response_promise<error>();
          return promise.delegate(worker, x);
        },
      };
    },
    worker);
  SECTION("handled messages are received by client") {
    auto client = sys.spawn([delegator](event_based_actor* self) {
      self->send(delegator, 2);
      self->send(delegator, 3);
      return behavior{[](int32_t) {}};
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
  SECTION("unhandled messages respond with error") {
    auto client = sys.spawn([count, delegator](event_based_actor* self) {
      self->send(delegator, "2");
      return behavior{[](int32_t) {}};
    });
    expect<std::string>().with("2").from(client).to(delegator);
    expect<std::string>().with("2").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
  }
  SECTION("consecutive messages are handled after unhandled message") {
    auto client = sys.spawn([count, delegator](event_based_actor* self) {
      self->send(delegator, "2");
      self->send(delegator, 3);
      self->send(delegator, 4);
      return behavior{[](int32_t) {}};
    });
    expect<std::string>().with("2").from(client).to(delegator);
    expect<std::string>().with("2").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
    expect<int32_t>().with(3).from(client).to(delegator);
    expect<int32_t>().with(3).from(client).to(worker);
    expect<int32_t>().with(3).from(worker).to(client);
    check_eq(*count, 3);
    expect<int32_t>().with(4).from(client).to(delegator);
    expect<int32_t>().with(4).from(client).to(worker);
    expect<int32_t>().with(4).from(worker).to(client);
    check_eq(*count, 7);
  }
  SECTION("consecutive unhandlded messages respond with error") {
    auto client = sys.spawn([count, delegator](event_based_actor* self) {
      self->send(delegator, "2");
      self->send(delegator, "3");
      self->send(delegator, "4");
      return behavior{[](int32_t) {}};
    });
    expect<std::string>().with("2").from(client).to(delegator);
    expect<std::string>().with("2").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
    expect<std::string>().with("3").from(client).to(delegator);
    expect<std::string>().with("3").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
    expect<std::string>().with("4").from(client).to(delegator);
    expect<std::string>().with("4").from(client).to(worker);
    expect<error>().with(sec::unexpected_message).from(worker).to(client);
  }
}
#endif // CAF_ENABLE_EXCEPTIONS

} // WITH_FIXTURE(fixture::deterministic)

CAF_TEST_MAIN()
