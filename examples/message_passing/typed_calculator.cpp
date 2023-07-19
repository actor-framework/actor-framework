// This example is a very basic, non-interactive math service implemented using
// typed actors.

#include "caf/all.hpp"

#include <cassert>
#include <iostream>

using std::endl;
using namespace caf;

namespace {

using calculator_type
  = typed_actor<result<int32_t>(add_atom, int32_t, int32_t),
                result<int32_t>(sub_atom, int32_t, int32_t)>;

calculator_type::behavior_type typed_calculator_fun(calculator_type::pointer) {
  return {
    [](add_atom, int32_t x, int32_t y) -> int32_t { return x + y; },
    [](sub_atom, int32_t x, int32_t y) -> int32_t { return x - y; },
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
      [](add_atom, int32_t x, int32_t y) -> int32_t { return x + y; },
      [](sub_atom, int32_t x, int32_t y) -> int32_t { return x - y; },
    };
  }
};

void tester(event_based_actor* self, const calculator_type& testee) {
  self->link_to(testee);
  // first test: 2 + 1 = 3
  self->request(testee, infinite, add_atom_v, 2, 1)
    .then(
      [=](int32_t r1) {
        // second test: 2 - 1 = 1
        self->request(testee, infinite, sub_atom_v, 2, 1).then([=](int32_t r2) {
          // both tests succeeded
          if (r1 == 3 && r2 == 1) {
            aout(self) << "AUT (actor under test) seems to be ok" << endl;
          }
          self->send_exit(testee, exit_reason::user_shutdown);
        });
      },
      [=](const error& err) {
        aout(self) << "AUT (actor under test) failed: " << to_string(err)
                   << endl;
        self->quit(exit_reason::user_shutdown);
      });
}

void caf_main(actor_system& system) {
  // test function-based impl
  system.spawn(tester, system.spawn(typed_calculator_fun));
  system.await_all_actors_done();
  // test class-based impl
  system.spawn(tester, system.spawn<typed_calculator_class>());
}

} // namespace

CAF_MAIN()
