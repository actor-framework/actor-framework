#include <chrono>
#include "test.hpp"
#include "caf/all.hpp"

using namespace caf;

void testee(event_based_actor* self) {
  auto counter = std::make_shared<int>(0);
  auto grp = group::get("local", "test");
  self->join(grp);
  CAF_CHECKPOINT();
  self->become(
    on(atom("msg")) >> [=] {
      CAF_CHECKPOINT();
      ++*counter;
      self->leave(grp);
      self->send(grp, atom("msg"));
    },
    on(atom("over")) >> [=] {
      // this actor should receive only 1 message
      CAF_CHECK(*counter == 1);
      self->quit();
    }
  );
  self->send(grp, atom("msg"));
  self->delayed_send(self, std::chrono::seconds(1), atom("over"));
}

int main() {
  CAF_TEST(test_local_group);
  spawn(testee);
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
