// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/detail/private_thread_pool.hpp"

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"

#include "caf/detail/private_thread.hpp"
#include "caf/log/test.hpp"
#include "caf/resumable.hpp"

using namespace caf;

namespace {

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
          using namespace std::literals::chrono_literals;
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

SCENARIO("private threads rerun their resumable when it returns resume_later") {
  struct testee : resumable {
    std::atomic<size_t> runs = 0;
    std::atomic<size_t> refs_added = 0;
    std::atomic<size_t> refs_released = 0;
    subtype_t subtype() const override {
      return resumable::function_object;
    }
    resume_result resume(execution_unit*, size_t) override {
      return ++runs < 2 ? resumable::resume_later : resumable::done;
    }
    void intrusive_ptr_add_ref_impl() override {
      ++refs_added;
    }
    void intrusive_ptr_release_impl() override {
      ++refs_released;
    }
  };
  GIVEN("a resumable f and a private thread t") {
    auto baseline = sys.detached_actors();
    testee f;
    auto t = sys.acquire_private_thread();
    WHEN("when resuming f with t") {
      t->resume(&f);
      THEN("t calls resume until f returns something other than resume_later") {
        using namespace std::literals::chrono_literals;
        sys.release_private_thread(t);
        while (f.runs != 2u)
          std::this_thread::sleep_for(1ms);
        while (sys.detached_actors() != baseline)
          std::this_thread::sleep_for(1ms);
        check_eq(f.refs_added, 0u);
        check_eq(f.refs_released, 1u);
      }
    }
  }
}

} // WITH_FIXTURE(test::fixture::deterministic)

} // namespace
