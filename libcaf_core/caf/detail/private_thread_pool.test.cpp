// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/private_thread_pool.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/detail/private_thread.hpp"
#include "caf/log/test.hpp"
#include "caf/resumable.hpp"

using namespace caf;
using namespace std::literals;

WITH_FIXTURE(test::fixture::deterministic) {

SCENARIO("private threads count towards detached actors") {
  GIVEN("an actor system with a private thread pool") {
    detail::private_thread* t1 = nullptr;
    detail::private_thread* t2 = nullptr;
    WHEN("acquiring and then releasing new private threads") {
      auto baseline = sys.detached_actors();
      log::test::debug("baseline: {}", baseline);
      THEN("the detached_actors counter increases") {
        check_eq(sys.detached_actors(), baseline);
        t1 = sys.acquire_private_thread();
        check_eq(sys.detached_actors(), baseline + 1);
        t2 = sys.acquire_private_thread();
        check_eq(sys.detached_actors(), baseline + 2);
      }
      AND_THEN("the detached_actors counter eventually decreases again") {
        auto next_value = [this, old_value = baseline + 2]() mutable {
          size_t result = 0;
          while ((result = sys.detached_actors()) == old_value)
            std::this_thread::sleep_for(1ms);
          old_value = result;
          return result;
        };
        sys.release_private_thread(t2);
        check_eq(next_value(), baseline + 1);
        sys.release_private_thread(t1);
        check_eq(next_value(), baseline);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)
