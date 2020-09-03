/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\******************************************************************************/

#include <cstdint>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

// --(rst-cell-begin)--
using cell = typed_actor<
  // 'put' updates the value of the cell.
  result<void>(put_atom, int32_t),
  // 'get' queries the value of the cell.
  result<int32_t>(get_atom)>;

struct cell_state {
  int32_t value = 0;
  static inline const char* name = "example.cell";
};

cell::behavior_type type_checked_cell(cell::stateful_pointer<cell_state> self) {
  return {
    [=](put_atom, int32_t val) { self->state.value = val; },
    [=](get_atom) { return self->state.value; },
  };
}

behavior unchecked_cell(stateful_actor<cell_state>* self) {
  return {
    [=](put_atom, int32_t val) { self->state.value = val; },
    [=](get_atom) { return self->state.value; },
  };
}
// --(rst-cell-end)--

void caf_main(actor_system& system) {
  // --(rst-spawn-cell-end)--
  // Create one cell for each implementation.
  auto cell1 = system.spawn(type_checked_cell);
  auto cell2 = system.spawn(unchecked_cell);
  // --(rst-spawn-cell-end)--
  auto f = make_function_view(cell1);
  cout << "cell value: " << f(get_atom_v) << endl;
  f(put_atom_v, 20);
  cout << "cell value (after setting to 20): " << f(get_atom_v) << endl;
  // Get an unchecked cell and send it some garbage. Triggers an "unexpected
  // message" error.
  anon_send(cell2, "hello there!");
}

CAF_MAIN()
