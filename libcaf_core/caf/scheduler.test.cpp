// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/scheduler.hpp"

#include "caf/test/outline.hpp"

#include "caf/actor_system_config.hpp"
#include "caf/add_ref.hpp"
#include "caf/resumable.hpp"

#include <latch>
#include <string>

using namespace caf;
using namespace std::literals;

namespace {

struct testee : resumable, ref_counted {
  explicit testee(std::shared_ptr<std::latch> latch_handle)
    : rendezvous(std::move(latch_handle)) {
  }

  void resume(scheduler* ctx, uint64_t event_id) override {
    if (event_id == resumable::dispose_event_id) {
      rendezvous->count_down();
      return;
    }
    if (++runs == 10) {
      rendezvous->count_down();
      return;
    }
    ctx->delay(resumable_ptr{this, add_ref}, resumable::default_event_id);
  }

  void ref_resumable() const noexcept final {
    ref();
  }

  void deref_resumable() const noexcept final {
    deref();
  }

  std::atomic<size_t> runs = 0;
  std::shared_ptr<std::latch> rendezvous;
};

} // namespace

OUTLINE("scheduling resumables") {
  GIVEN("an actor system using the work <sched> scheduler") {
    auto sched = block_parameters<std::string>();
    actor_system_config cfg;
    cfg.set("caf.scheduler.max-throughput", 5);
    cfg.set("caf.scheduler.max-threads", 2);
    cfg.set("caf.scheduler.policy", sched);
    WHEN("scheduling a resumable") {
      auto sys = std::make_unique<actor_system>(cfg);
      auto rendezvous = std::make_shared<std::latch>(2);
      auto uut = make_counted<testee>(rendezvous);
      sys->scheduler().schedule(uut, resumable::default_event_id);
      THEN("expect the resumable to be executed until done") {
        rendezvous->count_down();
        rendezvous->wait();
        check_eq(uut->runs.load(), 10u);
      }
      AND_THEN("the scheduler releases the ref when done") {
        // Note: destroying the actor system here will cause CAF to shut down.
        //       Ultimately stopping the scheduler and releasing the references.
        sys = nullptr;
        check(uut->unique());
      }
    }
    WHEN("scheduling multiple resumables") {
      auto sys = std::make_unique<actor_system>(cfg);
      auto testees = std::vector<intrusive_ptr<testee>>{};
      auto rendezvous = std::make_shared<std::latch>(11);
      for (int i = 0; i < 10; i++) {
        testees.emplace_back(make_counted<testee>(rendezvous));
        sys->scheduler().schedule(testees.back(), resumable::default_event_id);
      }
      THEN("expect the resumables to be executed until done") {
        rendezvous->count_down();
        rendezvous->wait();
        for (const auto& ptr : testees) {
          check_eq(ptr->runs, 10u);
        }
      }
      AND_THEN("the scheduler releases the ref when done") {
        // Note: destroying the actor system here will cause CAF to shut down.
        //       Ultimately stopping the scheduler and releasing the references.
        sys = nullptr;
        for (const auto& ptr : testees)
          check(ptr->unique());
      }
    }
  }
  EXAMPLES = R"(
    |    sched    |
    | sharing     |
    | stealing    |
  )";
}

struct awaiting_testee : resumable, ref_counted {
  explicit awaiting_testee(std::shared_ptr<std::latch> latch_handle)
    : rendezvous(std::move(latch_handle)) {
  }

  void resume(scheduler*, uint64_t event_id) override {
    if (event_id == resumable::dispose_event_id) {
      rendezvous->count_down();
      return;
    }
    ++runs;
    rendezvous->count_down();
  }

  void ref_resumable() const noexcept final {
    ref();
  }

  void deref_resumable() const noexcept final {
    deref();
  }

  std::atomic<size_t> runs = 0;
  std::shared_ptr<std::latch> rendezvous;
};

OUTLINE("scheduling units that are awaiting") {
  GIVEN("an actor system using the work <sched> scheduler") {
    auto sched = block_parameters<std::string>();
    actor_system_config cfg;
    cfg.set("caf.scheduler.policy", sched);
    cfg.set("caf.scheduler.max-threads", 2);
    cfg.set("caf.scheduler.max-throughput", 5);
    auto sys = std::make_unique<actor_system>(cfg);
    WHEN("having resumables that go to an awaiting state") {
      auto testees = std::vector<intrusive_ptr<awaiting_testee>>{};
      auto rendezvous = std::make_shared<std::latch>(11);
      for (int i = 0; i < 10; i++) {
        testees.push_back(make_counted<awaiting_testee>(rendezvous));
        sys->scheduler().schedule(testees.back(), resumable::default_event_id);
      }
      THEN("expect the resumables to be executed once") {
        rendezvous->count_down();
        rendezvous->wait();
        for (const auto& uut : testees) {
          check_eq(uut->runs, 1u);
        }
      }
      AND_THEN("the scheduler releases the ref when done") {
        // Note: destroying the actor system here will cause CAF to shut down.
        //       Ultimately stopping the scheduler and releasing the references.
        sys = nullptr;
        for (const auto& uut : testees)
          check(uut->unique());
      }
    }
  }
  EXAMPLES = R"(
    |    sched    |
    | sharing     |
    | stealing    |
  )";
}
