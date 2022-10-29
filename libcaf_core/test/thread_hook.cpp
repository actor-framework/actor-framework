// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE thread_hook

#include "caf/all.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

using atomic_count = std::atomic<size_t>;

size_t assumed_thread_count;
size_t assumed_init_calls;

struct dummy_thread_hook : thread_hook {
  void init(actor_system&) override {
    // nop
  }

  void thread_started(thread_owner) override {
    // nop
  }

  void thread_terminates() override {
    // nop
  }
};

class counting_thread_hook : public thread_hook {
public:
  counting_thread_hook()
    : count_init_{0}, count_thread_started_{0}, count_thread_terminates_{0} {
    // nop
  }

  ~counting_thread_hook() override {
    CHECK_EQ(count_init_, assumed_init_calls);
    CHECK_EQ(count_thread_started_, assumed_thread_count);
    CHECK_EQ(count_thread_terminates_, assumed_thread_count);
  }

  void init(actor_system&) override {
    ++count_init_;
  }

  void thread_started(thread_owner) override {
    ++count_thread_started_;
  }

  void thread_terminates() override {
    ++count_thread_terminates_;
  }

private:
  atomic_count count_init_;
  atomic_count count_thread_started_;
  atomic_count count_thread_terminates_;
};

template <class Hook>
struct config : actor_system_config {
  config() {
    add_thread_hook<Hook>();
    set("caf.logger.verbosity", "quiet");
  }
};

template <class Hook>
struct fixture {
  config<Hook> cfg;
  actor_system sys;
  fixture() : sys(cfg) {
    // nop
  }
};

} // namespace

CAF_TEST(counting_no_system) {
  assumed_init_calls = 0;
  actor_system_config cfg;
  cfg.add_thread_hook<counting_thread_hook>();
}

BEGIN_FIXTURE_SCOPE(fixture<dummy_thread_hook>)

CAF_TEST(counting_no_args) {
  // nop
}

END_FIXTURE_SCOPE()

BEGIN_FIXTURE_SCOPE(fixture<counting_thread_hook>)

CAF_TEST(counting_system_without_actor) {
  assumed_init_calls = 1;
  auto fallback = scheduler::abstract_coordinator::default_thread_count();
  assumed_thread_count = get_or(cfg, "caf.scheduler.max-threads", fallback)
                         + 2; // clock and private thread pool
  auto& sched = sys.scheduler();
  if (sched.detaches_utility_actors())
    assumed_thread_count += sched.num_utility_actors();
}

CAF_TEST(counting_system_with_actor) {
  assumed_init_calls = 1;
  auto fallback = scheduler::abstract_coordinator::default_thread_count();
  assumed_thread_count = get_or(cfg, "caf.scheduler.max-threads", fallback)
                         + 3; // clock, private thread pool, and  detached actor
  auto& sched = sys.scheduler();
  if (sched.detaches_utility_actors())
    assumed_thread_count += sched.num_utility_actors();
  sys.spawn<detached>([] {});
  sys.spawn([] {});
}

END_FIXTURE_SCOPE()
