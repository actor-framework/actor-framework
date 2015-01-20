#include <atomic>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

namespace {
std::atomic<long> s_testees;
} // namespace <anonymous>

class testee : public event_based_actor {
 public:
  testee() {
    ++s_testees;
  }

  ~testee();

  behavior make_behavior() {
    return {
      others() >> [=] {
        return last_dequeued();
      }
    };
  }
};

testee::~testee() {
  // avoid weak-vtables warning
  --s_testees;
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
    [self](const ExitMsgType&) {
      // must be still alive at this point
      CAF_CHECK_EQUAL(s_testees.load(), 1);
      // testee might be still running its cleanup code in
      // another worker thread; by waiting some milliseconds, we make sure
      // testee had enough time to return control to the scheduler
      // which in turn destroys it by dropping the last remaining reference
      self->delayed_send(self, std::chrono::milliseconds(30),
                         check_atom::value);
    },
    [self](check_atom) {
      // make sure dude's dtor has been called
      CAF_CHECK_EQUAL(s_testees.load(), 0);
      self->quit();
    }
  };
}

template <spawn_options O1, spawn_options O2>
void run() {
  CAF_PRINT("run test using links");
  spawn<O1>(tester<exit_msg>, spawn<testee, O2>());
  await_all_actors_done();
  CAF_PRINT("run test using monitors");
  spawn<O1>(tester<down_msg>, spawn<testee, O2>());
  await_all_actors_done();
}

void test_actor_lifetime() {
  CAF_PRINT("run<no_spawn_options, no_spawn_options>");
  run<no_spawn_options, no_spawn_options>();
  CAF_PRINT("run<detached, no_spawn_options>");
  run<detached, no_spawn_options>();
  CAF_PRINT("run<no_spawn_options, detached>");
  run<no_spawn_options, detached>();
  CAF_PRINT("run<detached, detached>");
  run<detached, detached>();
}

int main() {
  CAF_TEST(test_actor_lifetime);
  test_actor_lifetime();
  CAF_CHECK_EQUAL(s_testees.load(), 0);
  return CAF_TEST_RESULT();
}
