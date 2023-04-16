// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.private_thread_pool

#include "caf/detail/private_thread_pool.hpp"

#include "core-test.hpp"

#include "caf/detail/private_thread.hpp"

using namespace caf;

BEGIN_FIXTURE_SCOPE(test_coordinator_fixture<>)

SCENARIO("private threads count towards detached actors") {
  GIVEN("an actor system with a private thread pool") {
    detail::private_thread* t1 = nullptr;
    detail::private_thread* t2 = nullptr;
    WHEN("acquiring new private threads") {
      THEN("the detached_actors counter increases") {
        CHECK_EQ(sys.detached_actors(), 0u);
        t1 = sys.acquire_private_thread();
        CHECK_EQ(sys.detached_actors(), 1u);
        t2 = sys.acquire_private_thread();
        CHECK_EQ(sys.detached_actors(), 2u);
      }
    }
    WHEN("releasing the private threads") {
      THEN("the detached_actors counter eventually decreases again") {
        auto next_value = [this, old_value{2u}]() mutable {
          using namespace std::literals::chrono_literals;
          size_t result = 0;
          while ((result = sys.detached_actors()) == old_value)
            std::this_thread::sleep_for(1ms);
          old_value = result;
          return result;
        };
        sys.release_private_thread(t2);
        CHECK_EQ(next_value(), 1u);
        sys.release_private_thread(t1);
        CHECK_EQ(next_value(), 0u);
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
    testee f;
    auto t = sys.acquire_private_thread();
    WHEN("when resuming f with t") {
      t->resume(&f);
      THEN("t calls resume until f returns something other than resume_later") {
        using namespace std::literals::chrono_literals;
        sys.release_private_thread(t);
        while (f.runs != 2u)
          std::this_thread::sleep_for(1ms);
        while (sys.detached_actors() != 0)
          std::this_thread::sleep_for(1ms);
        CHECK_EQ(f.refs_added, 0u);
        CHECK_EQ(f.refs_released, 1u);
      }
    }
  }
}

END_FIXTURE_SCOPE()
