/******************************************************************************\
 * Illustrates response promises.                                             *
\******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 18-43 (MessagePassing.tex)

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;

using namespace caf;

// using add_atom = atom_constant<atom("add")>; (defined in atom.hpp)

using adder = typed_actor<replies_to<add_atom, int, int>::with<int>>;

// function-based, statically typed, event-based API
adder::behavior_type worker() {
  return {
    [](add_atom, int a, int b) {
      return a + b;
    }
  };
}

// function-based, statically typed, event-based API
adder::behavior_type calculator_master(adder::pointer self) {
  auto w = self->spawn(worker);
  return {
    [=](add_atom x, int y, int z) -> result<int> {
      auto rp = self->make_response_promise<int>();
      self->request(w, infinite, x, y, z).then([=](int result) mutable {
        rp.deliver(result);
      });
      return rp;
    }
  };
}

void caf_main(actor_system& system) {
  auto f = make_function_view(system.spawn(calculator_master));
  cout << "12 + 13 = " << f(add_atom_v, 12, 13) << endl;
}

CAF_MAIN()
