// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/system_messages.hpp"

#include "caf/test/caf_test_main.hpp"
#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/test.hpp"

#include "caf/event_based_actor.hpp"

using namespace caf;

WITH_FIXTURE(test::fixture::deterministic) {

TEST("exit_msg is comparable") {
  SECTION("invalid actor address") {
    auto msg1 = exit_msg{nullptr, sec::runtime_error};
    auto msg2 = exit_msg{nullptr, sec::runtime_error};
    auto msg3 = exit_msg{nullptr, sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
  SECTION("valid actor address") {
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto msg1 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg2 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg3 = exit_msg{dummy.address(), sec::disposed};
    check_eq(msg1, msg2);
    check_eq(msg2, msg1);
    check_ne(msg1, msg3);
    check_ne(msg3, msg1);
  }
}

TEST("exit_msg is serializable") {
  SECTION("invalid actor address") {
    auto msg1 = exit_msg{nullptr, sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
  SECTION("valid actor address") {
    // Note: serializing an actor puts it into the registry. Hence, we need to
    //       shut down the actor manually before the end because its reference
    //       count will not drop to zero otherwise.
    auto dummy = sys.spawn([] { return behavior{[](int) {}}; });
    auto dummy_guard = make_actor_scope_guard(dummy);
    auto msg1 = exit_msg{dummy.address(), sec::runtime_error};
    auto msg2 = serialization_roundtrip(msg1);
    check_eq(msg1, msg2);
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

CAF_TEST_MAIN()
