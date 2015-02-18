/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\ ******************************************************************************/

#include <tuple>
#include <cassert>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;

// implementation using the blocking API
void blocking_calculator(blocking_actor* self) {
  self->receive_loop (
    [](plus_atom, int a, int b) {
      return a + b;
    },
    [](minus_atom, int a, int b) {
      return a - b;
    },
    others() >> [=] {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  );
}

// execute this behavior until actor terminates
behavior calculator(event_based_actor* self) {
  return behavior{
    [](plus_atom, int a, int b) {
      return a + b;
    },
    [](minus_atom, int a, int b) {
      return a - b;
    },
    others() >> [=] {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  };
}

void tester(event_based_actor* self, const actor& testee) {
  self->link_to(testee);
  // will be invoked if we receive an unexpected response message
  self->on_sync_failure([=] {
    aout(self) << "AUT (actor under test) failed" << endl;
    self->quit(exit_reason::user_shutdown);
  });
  // first test: 2 + 1 = 3
  self->sync_send(testee, plus_atom::value, 2, 1).then(
    on(3) >> [=] {
      // second test: 2 - 1 = 1
      self->sync_send(testee, minus_atom::value, 2, 1).then(
        on(1) >> [=] {
          // both tests succeeded
          aout(self) << "AUT (actor under test) seems to be ok" << endl;
          self->quit(exit_reason::user_shutdown);
        }
      );
    }
  );
}

int main() {
  cout << "test blocking actor" << endl;
  spawn(tester, spawn<blocking_api>(blocking_calculator));
  await_all_actors_done();
  cout << "test event-based actor" << endl;
  spawn(tester, spawn(calculator));
  await_all_actors_done();
  shutdown();
}
