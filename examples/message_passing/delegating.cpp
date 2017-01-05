#include <iostream>
#include "caf/all.hpp"

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 15-42 (MessagePassing.tex)

using std::endl;
using namespace caf;

// using add_atom = atom_constant<atom("add")>; (defined in atom.hpp)

using calc = typed_actor<replies_to<add_atom, int, int>::with<int>>;

void actor_a(event_based_actor* self, const calc& worker) {
  self->request(worker, std::chrono::seconds(10), add_atom::value, 1, 2).then(
    [=](int result) {
      aout(self) << "1 + 2 = " << result << endl;
    }
  );
}

calc::behavior_type actor_b(calc::pointer self, const calc& worker) {
  return {
    [=](add_atom add, int x, int y) {
      return self->delegate(worker, add, x, y);
    }
  };
}

calc::behavior_type actor_c() {
  return {
    [](add_atom, int x, int y) {
      return x + y;
    }
  };
}

void caf_main(actor_system& system) {
  system.spawn(actor_a, system.spawn(actor_b, system.spawn(actor_c)));
}

CAF_MAIN()
