#include <chrono>
#include "test.hpp"
#include "caf/all.hpp"

using namespace caf;

using msg_atom = atom_constant<atom("msg")>;
using timeout_atom = atom_constant<atom("timeout")>;

void testee(event_based_actor* self) {
  auto counter = std::make_shared<int>(0);
  auto grp = group::get("local", "test");
  self->join(grp);
  CAF_CHECKPOINT();
  self->become(
    [=](msg_atom) {
      CAF_CHECKPOINT();
      ++*counter;
      self->leave(grp);
      self->send(grp, msg_atom::value);
    },
    [=](timeout_atom) {
      // this actor should receive only 1 message
      CAF_CHECK(*counter == 1);
      self->quit();
    }
  );
  self->send(grp, msg_atom::value);
  self->delayed_send(self, std::chrono::seconds(1), timeout_atom::value);
}

int main() {
  CAF_TEST(test_local_group);
  spawn(testee);
  await_all_actors_done();
  shutdown();
  return CAF_TEST_RESULT();
}
