/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 18-44, and 49-50 (Actor.tex)

#include <cstdint>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

using cell = typed_actor<reacts_to<put_atom, int32_t>,
                         replies_to<get_atom>::with<int32_t>>;

struct cell_state {
  int32_t value = 0;
};

cell::behavior_type type_checked_cell(cell::stateful_pointer<cell_state> self) {
  return {[=](put_atom, int32_t val) { self->state.value = val; },
          [=](get_atom) { return self->state.value; }};
}

behavior unchecked_cell(stateful_actor<cell_state>* self) {
  return {[=](put_atom, int32_t val) { self->state.value = val; },
          [=](get_atom) { return self->state.value; }};
}

void caf_main(actor_system& system) {
  // create one cell for each implementation
  auto cell1 = system.spawn(type_checked_cell);
  auto cell2 = system.spawn(unchecked_cell);
  auto f = make_function_view(cell1);
  cout << "cell value: " << f(get_atom_v) << endl;
  f(put_atom_v, 20);
  cout << "cell value (after setting to 20): " << f(get_atom_v) << endl;
  // get an unchecked cell and send it some garbage
  anon_send(cell2, "hello there!");
}

CAF_MAIN()
