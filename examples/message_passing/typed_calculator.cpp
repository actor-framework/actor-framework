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

calculator_type::behavior_type typed_calculator_fun(calculator_type::pointer) {
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
public:
  typed_calculator_class(actor_config& cfg) : calculator_type::base(cfg) {
    // nop
  }

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
  // first test: 2 + 1 = 3
  self->request(testee, infinite, plus_atom::value, 2, 1).then(
    [=](result_atom, int r1) {
      // second test: 2 - 1 = 1
      self->request(testee, infinite, minus_atom::value, 2, 1).then(
        [=](result_atom, int r2) {
          // both tests succeeded
          if (r1 == 3 && r2 == 1) {
            aout(self) << "AUT (actor under test) seems to be ok"
                       << endl;
          }
          self->send_exit(testee, exit_reason::user_shutdown);
        }
      );
    },
    [=](const error& err) {
      aout(self) << "AUT (actor under test) failed: "
                 << self->system().render(err) << endl;
      self->quit(exit_reason::user_shutdown);
    }
  );
}

void caf_main(actor_system& system) {
  // test function-based impl
  system.spawn(tester, system.spawn(typed_calculator_fun));
  system.await_all_actors_done();
  // test class-based impl
  system.spawn(tester, system.spawn<typed_calculator_class>());
}

} // namespace <anonymous>

CAF_MAIN()
