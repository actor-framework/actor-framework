#include <atomic>
#include <iostream>

#include "test.hpp"

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

std::atomic<long> s_dudes;

class dude : public event_based_actor {
 public:
  dude() {
    ++s_dudes;
  }

  ~dude() {
    --s_dudes;
  }

  behavior make_behavior() {
    return {
      others() >> [=] {
        return last_dequeued();
      }
    };
  }
};

behavior linking_dude(event_based_actor* self, const actor& other_dude) {
  CAF_CHECKPOINT();
  self->trap_exit(true);
  self->link_to(other_dude);
  anon_send_exit(other_dude, exit_reason::user_shutdown);
  CAF_CHECKPOINT();
  return {
    [self](const exit_msg&) {
      CAF_CHECKPOINT();
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
      CAF_CHECKPOINT();
      self->send(self, atom("check"));
    },
    on(atom("check")) >> [self] {
      // make sure dude's dtor has been called
      CAF_CHECK_EQUAL(s_dudes.load(), 0);
      self->quit();
    }
  };
}

void test_actor_lifetime() {
  spawn(monitoring_dude, spawn<dude>());
  await_all_actors_done();
  spawn(linking_dude, spawn<dude>());
  await_all_actors_done();
}

int main() {
  CAF_TEST(test_actor_lifetime);
  test_actor_lifetime();
  return CAF_TEST_RESULT();
}
