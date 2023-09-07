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
TEST("delegates pass on the response properly") {
  using namespace caf;
  auto worker = sys.spawn(
    [] { return behavior{[](int32_t value) { return value * 2; }}; });
  auto delegator = sys.spawn(
    [](event_based_actor* self, caf::actor worker) {
      return caf::behavior{
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
  scoped_actor self{sys};
  SECTION("successful requests are propagated to sender") {
    self->send(delegator, 2);
    check_eq(mail_count(delegator), 1u);
    check(dispatch_messages());
    self->receive([&](int32_t result) { check_eq(result, 4); });
  }
  SECTION("failed requests are propagated to sender") {
    self->send(delegator, "2");
    check_eq(mail_count(delegator), 1u);
    check(dispatch_messages());
    self->receive([&](caf::error& err) {
      check_eq(err, make_error(sec::runtime_error, "Invalid type"));
    });
    scoped_actor self{sys};
    SECTION("delegator delegates responses to the sender") {
      SECTION("successful requests are propagated to sender") {
        self->send(delegator, 2);
        check_eq(mail_count(delegator), 1u);
        check(dispatch_messages());
        self->receive([&](int32_t result) { check_eq(result, 4); });
      }
      SECTION("failed requests are propagated to sender") {
        self->send(delegator, "2");
        check_eq(mail_count(delegator), 1u);
        check(dispatch_messages());
        self->receive([&](caf::error& err) {
          check_eq(err, make_error(sec::runtime_error, "Invalid type"));
        });
      }
    }
  }
#endif // CAF_ENABLE_EXCEPTIONS
}

} // WITH_FIXTURE(fixture::deterministic)

CAF_TEST_MAIN()
