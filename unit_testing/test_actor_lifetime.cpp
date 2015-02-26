#include <atomic>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

namespace {
std::atomic<long> s_testees;
std::atomic<long> s_pending_on_exits;
} // namespace <anonymous>

class testee : public event_based_actor {
 public:
  testee();
  ~testee();
  void on_exit();
  behavior make_behavior() override;
};

testee::testee() {
  ++s_testees;
  ++s_pending_on_exits;
}

testee::~testee() {
  // avoid weak-vtables warning
  --s_testees;
}

void testee::on_exit() {
  --s_pending_on_exits;
}

behavior testee::make_behavior() {
  return {
    others >> [=] {
      return current_message();
    }
  };
}

template <class ExitMsgType>
behavior tester(event_based_actor* self, const actor& aut) {
  CAF_CHECKPOINT();
  if (std::is_same<ExitMsgType, exit_msg>::value) {
    self->trap_exit(true);
    self->link_to(aut);
  } else {
    self->monitor(aut);
  }
  CAF_CHECKPOINT();
  anon_send_exit(aut, exit_reason::user_shutdown);
  CAF_CHECKPOINT();
  return {
    [self](const ExitMsgType& msg) {
      // must be still alive at this point
      CAF_CHECK_EQUAL(s_testees.load(), 1);
      CAF_CHECK_EQUAL(msg.reason, exit_reason::user_shutdown);
      CAF_CHECK_EQUAL(self->current_message().vals()->get_reference_count(), 1);
      CAF_CHECK(&msg == self->current_message().at(0));
      // testee might be still running its cleanup code in
      // another worker thread; by waiting some milliseconds, we make sure
      // testee had enough time to return control to the scheduler
      // which in turn destroys it by dropping the last remaining reference
      self->delayed_send(self, std::chrono::milliseconds(30),
                         check_atom::value);
    },
    [self](check_atom) {
      // make sure aut's dtor and on_exit() have been called
      CAF_CHECK_EQUAL(s_testees.load(), 0);
      CAF_CHECK_EQUAL(s_pending_on_exits.load(), 0);
      self->quit();
    }
  };
}

#define BREAK_ON_ERROR() if (CAF_TEST_RESULT() > 0) return

template <spawn_options O1, spawn_options O2>
void run() {
  CAF_PRINT("run test using links");
  spawn<O1>(tester<exit_msg>, spawn<testee, O2>());
  await_all_actors_done();
  BREAK_ON_ERROR();
  CAF_PRINT("run test using monitors");
  spawn<O1>(tester<down_msg>, spawn<testee, O2>());
  await_all_actors_done();
}

void test_actor_lifetime() {
  CAF_PRINT("run<no_spawn_options, no_spawn_options>");
  run<no_spawn_options, no_spawn_options>();
  BREAK_ON_ERROR();
  CAF_PRINT("run<detached, no_spawn_options>");
  run<detached, no_spawn_options>();
  BREAK_ON_ERROR();
  CAF_PRINT("run<no_spawn_options, detached>");
  run<no_spawn_options, detached>();
  BREAK_ON_ERROR();
  CAF_PRINT("run<detached, detached>");
  run<detached, detached>();
}

int main() {
  CAF_TEST(test_actor_lifetime);
  test_actor_lifetime();
  shutdown();
  return CAF_TEST_RESULT();
}
