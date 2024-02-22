// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE actor_lifetime

#include "caf/all.hpp"

#include "core-test.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>

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
      [=](int x) { return x; },
    };
  }
};

template <class ExitMsgType>
behavior tester(event_based_actor* self, const actor& aut) {
  if (std::is_same_v<ExitMsgType, exit_msg>) {
    self->set_exit_handler([self](exit_msg& msg) {
      // must be still alive at this point
      CHECK_EQ(s_testees.load(), 1);
      CHECK_EQ(msg.reason, exit_reason::user_shutdown);
      self->mail(ok_atom_v).send(self);
    });
    self->link_to(aut);
  } else {
    self->set_down_handler([self](down_msg& msg) {
      // must be still alive at this point
      CHECK_EQ(s_testees.load(), 1);
      CHECK_EQ(msg.reason, exit_reason::user_shutdown);
      // testee might be still running its cleanup code in
      // another worker thread; by waiting some milliseconds, we make sure
      // testee had enough time to return control to the scheduler
      // which in turn destroys it by dropping the last remaining reference
      self->mail(ok_atom_v).send(self);
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
    [self](ok_atom) {
      { // make sure aut's dtor and on_exit() have been called
        std::unique_lock<std::mutex> guard{s_mtx};
        while (!s_testee_cleanup_done.load())
          s_cv.wait(guard);
      }
      CHECK_EQ(s_testees.load(), 0);
      CHECK_EQ(s_pending_on_exits.load(), 0);
      self->quit();
    },
  };
}

struct fixture : public test_coordinator_fixture<> {
  template <spawn_options Os, class... Ts>
  actor spawn(Ts&&... xs) {
    return sys.spawn<Os>(xs...);
  }

  template <class T, spawn_options Os, class... Ts>
  actor spawn(Ts&&... xs) {
    return sys.spawn<T, Os>(xs...);
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
      // expect((exit_msg), from(tst_driver).to(tst_subject));
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

} // namespace

CAF_TEST(destructor_call) {
  { // lifetime scope of actor system
    actor_system_config cfg;
    actor_system system{cfg};
    system.spawn<testee>();
  }
  CHECK_EQ(s_testees.load(), 0);
  CHECK_EQ(s_pending_on_exits.load(), 0);
}

BEGIN_FIXTURE_SCOPE(fixture)

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

END_FIXTURE_SCOPE()
