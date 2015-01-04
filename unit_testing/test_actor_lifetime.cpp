#include <atomic>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

namespace {
std::atomic<long> s_dudes;
} // namespace <anonymous>

class dude : public event_based_actor {
 public:
  dude() {
    ++s_dudes;
  }

  ~dude();

  behavior make_behavior() {
    return {
      others() >> [=] {
        return last_dequeued();
      }
    };
  }
};

dude::~dude() {
  // avoid weak-vtables warning
  --s_dudes;
}

behavior linking_dude(event_based_actor* self, const actor& other_dude) {
  CAF_CHECKPOINT();
  self->trap_exit(true);
  self->link_to(other_dude);
  anon_send_exit(other_dude, exit_reason::user_shutdown);
  CAF_CHECKPOINT();
  return {
    [self](const exit_msg&) {
      // must not been destroyed here
      CAF_CHECK_EQUAL(s_dudes.load(), 1);
      self->send(self, atom("check"));
    },
    on(atom("check")) >> [self] {
      // make sure dude's dtor has been called
      CAF_CHECK_EQUAL(s_dudes.load(), 0);
      self->quit();
    }
  };
}

behavior monitoring_dude(event_based_actor* self, actor other_dude) {
  CAF_CHECKPOINT();
  self->monitor(other_dude);
  anon_send_exit(other_dude, exit_reason::user_shutdown);
  CAF_CHECKPOINT();
  return {
    [self](const down_msg&) {
      // must not been destroyed here
      CAF_CHECK_EQUAL(s_dudes.load(), 1);
      self->send(self, atom("check"));
    },
    on(atom("check")) >> [self] {
      // make sure dude's dtor has been called
      CAF_CHECK_EQUAL(s_dudes.load(), 0);
      self->quit();
    }
  };
}

template <spawn_options O1, spawn_options O2>
void run() {
  spawn<O1>(linking_dude, spawn<dude, O2>());
  await_all_actors_done();
  spawn<O1>(monitoring_dude, spawn<dude, O2>());
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
  CAF_CHECK_EQUAL(s_dudes.load(), 0);
  return CAF_TEST_RESULT();
}
