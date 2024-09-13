// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/detail/latch.hpp"

#include "caf/test/scenario.hpp"

#include <thread>

using namespace caf;

SCENARIO("latches synchronize threads") {
  GIVEN("a latch and three threads") {
    detail::latch sync{2};
    std::vector<std::thread> threads;
    WHEN("synchronizing the threads via the latch") {
      THEN("wait() blocks until all threads counted down the latch") {
        threads.emplace_back([&sync] { sync.count_down(); });
        threads.emplace_back([&sync] { sync.count_down_and_wait(); });
        threads.emplace_back([&sync] { sync.wait(); });
        sync.wait();
        check(sync.is_ready());
      }
    }
    for (auto& t : threads)
      t.join();
  }
}
