/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * using typed actors.                                                        *
\ ******************************************************************************/

#include <cassert>
#include <iostream>
#include "caf/all.hpp"

using std::endl;
using namespace caf;

namespace {

using plus_atom = atom_constant<atom("plus")>;
using minus_atom = atom_constant<atom("minus")>;
using result_atom = atom_constant<atom("result")>;

using calculator_type =
  typed_actor<replies_to<plus_atom, int, int>::with<result_atom, int>,
              replies_to<minus_atom, int, int>::with<result_atom, int>>;

calculator_type::behavior_type typed_calculator(calculator_type::pointer) {
  return {
    [](plus_atom, int x, int y) {
      return std::make_tuple(result_atom::value, x + y);
    },
    [](minus_atom, int x, int y) {
      return std::make_tuple(result_atom::value, x - y);
    }
  };
}

class typed_calculator_class : public calculator_type::base {
protected:
  behavior_type make_behavior() override {
    return {
      [](plus_atom, int x, int y) {
        return std::make_tuple(result_atom::value, x + y);
      },
      [](minus_atom, int x, int y) {
        return std::make_tuple(result_atom::value, x - y);
      }
    };
  }
};

void tester(event_based_actor* self, const calculator_type& testee) {
  self->link_to(testee);
  // will be invoked if we receive an unexpected response message
  self->on_sync_failure([=] {
    aout(self) << "AUT (actor under test) failed" << endl;
    self->quit(exit_reason::user_shutdown);
  });
  // first test: 2 + 1 = 3
  self->sync_send(testee, plus_atom::value, 2, 1).then(
    [=](result_atom, int r1) {
      // second test: 2 - 1 = 1
      self->sync_send(testee, minus_atom::value, 2, 1).then(
        [=](result_atom, int r2) {
          // both tests succeeded
          if (r1 == 3 && r2 == 1) {
            aout(self) << "AUT (actor under test) seems to be ok"
                       << endl;
          }
          self->send_exit(testee, exit_reason::user_shutdown);
        }
      );
    }
  );
}

} // namespace <anonymous>

int main() {
  // test function-based impl
  spawn(tester, spawn(typed_calculator));
  await_all_actors_done();
  // test class-based impl
  spawn(tester, spawn<typed_calculator_class>());
  await_all_actors_done();
  // done
  shutdown();
}
