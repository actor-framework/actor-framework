/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

// this suite tests whether actors terminate as expect in several use cases
#define CAF_SUITE thread_hook

#include "caf/all.hpp"
#include "caf/test/unit_test.hpp"

using namespace caf;

namespace {

using atomic_count = std::atomic<size_t>;

size_t assumed_thread_count;
size_t assumed_init_calls;

std::mutex mx;

struct dummy_thread_hook : thread_hook {
  void init(actor_system&) override {
    // nop
  }

  void thread_started() override {
    // nop
  }

  void thread_terminates() override {
    // nop
  }
};

class counting_thread_hook : public thread_hook {
public:
  counting_thread_hook()
      : count_init_{0},
        count_thread_started_{0},
        count_thread_terminates_{0} {
    // nop
  }

  ~counting_thread_hook() override {
    CAF_CHECK_EQUAL(count_init_, assumed_init_calls);
    CAF_CHECK_EQUAL(count_thread_started_, assumed_thread_count);
    CAF_CHECK_EQUAL(count_thread_terminates_, assumed_thread_count);
  }

  void init(actor_system&) override {
    ++count_init_;
  }

  void thread_started() override {
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
    set("logger.verbosity", atom("quiet"));
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

} // namespace <anonymous>

CAF_TEST(counting_no_system) {
  assumed_init_calls = 0;
  actor_system_config cfg;
  cfg.add_thread_hook<counting_thread_hook>();
}

CAF_TEST_FIXTURE_SCOPE(dummy_hook, fixture<dummy_thread_hook>)

CAF_TEST(counting_no_args) {
  // nop
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(counting_hook, fixture<counting_thread_hook>)

CAF_TEST(counting_system_without_actor) {
  assumed_init_calls = 1;
  assumed_thread_count = get_or(cfg, "scheduler.max-threads",
                                defaults::scheduler::max_threads)
                         + 1; // caf.clock
  auto& sched = sys.scheduler();
  if (sched.detaches_utility_actors())
    assumed_thread_count += sched.num_utility_actors();
}

CAF_TEST(counting_system_with_actor) {
  assumed_init_calls = 1;
  assumed_thread_count = get_or(cfg, "scheduler.max-threads",
                                defaults::scheduler::max_threads)
                         + 2; // caf.clock and detached actor
  auto& sched = sys.scheduler();
  if (sched.detaches_utility_actors())
    assumed_thread_count += sched.num_utility_actors();
  sys.spawn<detached>([] {});
  sys.spawn([] {});
}

CAF_TEST_FIXTURE_SCOPE_END()

