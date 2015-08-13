/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\ ******************************************************************************/

#include <tuple>
#include <cassert>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::cerr;
using std::endl;
using namespace caf;

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;
using result_atom = atom_constant<atom("result")>;

using calculator_actor =
  typed_actor<replies_to<plus_atom, int, int>::with<result_atom, int>,
              replies_to<minus_atom, int, int>::with<result_atom, int>>;

// implementation using the blocking API
void blocking_calculator(blocking_actor* self) {
  self->receive_loop (
    [](plus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a + b);
    },
    [](minus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a - b);
    },
    others >> [=] {
      cout << "unexpected: " << to_string(self->current_message()) << endl;
    }
  );
}

// implementation using the event-based API
behavior calculator(event_based_actor* self) {
  return behavior{
    [](plus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a + b);
    },
    [](minus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a - b);
    },
    others >> [=] {
      cerr << "unexpected: " << to_string(self->current_message()) << endl;
    }
  };
}

// implementation using the statically typed API
calculator_actor::behavior_type typed_calculator() {
  return {
    [](plus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a + b);
    },
    [](minus_atom, int a, int b) {
      return std::make_tuple(result_atom::value, a - b);
    }
  };
}

// tests a calculator instance
template <class Handle>
void tester(event_based_actor* self, const Handle& testee, int x, int y) {
  self->link_to(testee);
  // will be invoked if we receive an unexpected response message
  self->on_sync_failure([=] {
    aout(self) << "AUT (actor under test) failed" << endl;
    self->quit(exit_reason::user_shutdown);
  });
  // first test: 2 + 1 = 3
  self->sync_send(testee, plus_atom::value, x, y).then(
    [=](result_atom, int res1) {
      aout(self) << x << " + " << y << " = " << res1 << endl;
      self->sync_send(testee, minus_atom::value, x, y).then(
        [=](result_atom, int res2) {
          // both tests succeeded
        aout(self) << x << " - " << y << " = " << res2 << endl;
          self->quit(exit_reason::user_shutdown);
        }
      );
    }
  );
}

void test_calculators() {
  scoped_actor self;
  aout(self) << "blocking actor:" << endl;
  self->spawn(tester<actor>, spawn<blocking_api>(blocking_calculator), 1, 2);
  self->await_all_other_actors_done();
  aout(self) << "event-based actor:" << endl;
  self->spawn(tester<actor>, spawn(calculator), 3, 4);
  self->await_all_other_actors_done();
  aout(self) << "typed actor:" << endl;
  self->spawn(tester<calculator_actor>, spawn(typed_calculator), 5, 6);
  self->await_all_other_actors_done();
}

int main() {
  test_calculators();
  shutdown();
}
