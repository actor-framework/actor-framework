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

#define CAF_SUITE actor_lifetime
#include "caf/test/unit_test.hpp"

#include <mutex>
#include <atomic>
#include <condition_variable>

#include "caf/all.hpp"

#include "caf/test/dsl.hpp"

using check_atom = caf::atom_constant<caf::atom("check")>;

using namespace caf;

namespace {

std::mutex s_mtx;
std::condition_variable s_cv;
std::atomic<bool> s_tester_init_done;
std::atomic<bool> s_testee_cleanup_done;

std::atomic<long> s_testees;
std::atomic<long> s_pending_on_exits;

class testee : public event_based_actor {
public:
  testee(actor_config& cfg) : event_based_actor(cfg) {
    ++s_testees;
    ++s_pending_on_exits;
  }

  ~testee() override {
    --s_testees;
  }

  const char* name() const override {
    return "testee";
  }

  void on_exit() override {
    --s_pending_on_exits;
  }

  behavior make_behavior() override {
    return {
      [=](int x) {
        return x;
      }
    };
  }
};

template <class ExitMsgType>
behavior tester(event_based_actor* self, const actor& aut) {
  if (std::is_same<ExitMsgType, exit_msg>::value) {
    self->set_exit_handler([self](exit_msg& msg) {
      // must be still alive at this point
      CAF_CHECK_EQUAL(s_testees.load(), 1);
      CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
      self->send(self, check_atom::value);
    });
    self->link_to(aut);
  } else {
    self->set_down_handler([self](down_msg& msg) {
      // must be still alive at this point
      CAF_CHECK_EQUAL(s_testees.load(), 1);
      CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
      // testee might be still running its cleanup code in
      // another worker thread; by waiting some milliseconds, we make sure
      // testee had enough time to return control to the scheduler
      // which in turn destroys it by dropping the last remaining reference
      self->send(self, check_atom::value);
    });
    self->monitor(aut);
  }
  anon_send_exit(aut, exit_reason::user_shutdown);
  {
    std::unique_lock<std::mutex> guard{s_mtx};
    s_tester_init_done = true;
    s_cv.notify_one();
  }
  return {
    [self](check_atom) {
      { // make sure aut's dtor and on_exit() have been called
        std::unique_lock<std::mutex> guard{s_mtx};
        while (!s_testee_cleanup_done.load())
          s_cv.wait(guard);
      }
      CAF_CHECK_EQUAL(s_testees.load(), 0);
      CAF_CHECK_EQUAL(s_pending_on_exits.load(), 0);
      self->quit();
    }
  };
}



struct config : actor_system_config {
  config() {
    set("scheduler.policy", atom("testing"));
  }
};

struct fixture {
  using sched_t = scheduler::test_coordinator;

  config cfg;
  actor_system system;
  sched_t& sched;

  fixture() : system(cfg), sched(dynamic_cast<sched_t&>(system.scheduler())) {
    // nop
  }

  template <spawn_options Os, class... Ts>
  actor spawn(Ts&&... xs) {
    return system.spawn<Os>(xs...);
  }

  template <class T, spawn_options Os, class... Ts>
  actor spawn(Ts&&... xs) {
    return system.spawn<T, Os>(xs...);
  }

  template <class ExitMsgType, spawn_options TesterOptions,
            spawn_options TesteeOptions>
  void tst() {
    // We re-use these static variables with each run.
    s_tester_init_done = false;
    s_testee_cleanup_done = false;
    // Spawn test subject and tester.
    auto tst_subject = spawn<testee, TesteeOptions>();
    sched.run();
    auto tst_driver = spawn<TesterOptions>(tester<ExitMsgType>, tst_subject);
    tst_subject = nullptr;
    if (has_detach_flag(TesterOptions)) {
      // When dealing with a detached tester we need to insert two
      // synchronization points: 1) exit_msg sent and 2) cleanup code of tester
      // done.
      { // Wait for the exit_msg from the driver.
        std::unique_lock<std::mutex> guard{s_mtx};
        while (!s_tester_init_done)
          s_cv.wait(guard);
      }
      // Run the exit_msg.
      sched.run_once();
      //expect((exit_msg), from(tst_driver).to(tst_subject));
      { // Resume driver.
        std::unique_lock<std::mutex> guard{s_mtx};
        s_testee_cleanup_done = true;
        s_cv.notify_one();
      }
    } else {
      // When both actors are running in the scheduler we don't need any extra
      // synchronization.
      s_tester_init_done = true;
      s_testee_cleanup_done = true;
      sched.run();
    }
  }
};

} // namespace <anonymous>

CAF_TEST(destructor_call) {
  { // lifetime scope of actor systme
    actor_system_config cfg;
    actor_system system{cfg};
    system.spawn<testee>();
  }
  CAF_CHECK_EQUAL(s_testees.load(), 0);
  CAF_CHECK_EQUAL(s_pending_on_exits.load(), 0);
}

CAF_TEST_FIXTURE_SCOPE(actor_lifetime_tests, fixture)

CAF_TEST(no_spawn_options_and_exit_msg) {
  tst<exit_msg, no_spawn_options, no_spawn_options>();
}

CAF_TEST(no_spawn_options_and_down_msg) {
  tst<down_msg, no_spawn_options, no_spawn_options>();
}

CAF_TEST(mixed_spawn_options_and_exit_msg) {
  tst<exit_msg, detached, no_spawn_options>();
}

CAF_TEST(mixed_spawn_options_and_down_msg) {
  tst<down_msg, detached, no_spawn_options>();
}

CAF_TEST_FIXTURE_SCOPE_END()
