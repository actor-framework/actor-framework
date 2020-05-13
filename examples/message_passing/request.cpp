// Illustrates semantics of request().{then|await|receive}.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "caf/all.hpp"

using std::endl;
using std::vector;
using std::chrono::seconds;

using namespace caf;

// --(rst-cell-begin)--
using cell = typed_actor<reacts_to<put_atom, int32_t>,
                         replies_to<get_atom>::with<int32_t>>;

struct cell_state {
  int32_t value = 0;
};

cell::behavior_type cell_impl(cell::stateful_pointer<cell_state> self,
                              int32_t x0) {
  self->state.value = x0;
  return {
    [=](put_atom, int32_t val) { self->state.value = val; },
    [=](get_atom) { return self->state.value; },
  };
}

void waiting_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom_v).await([=](int32_t y) {
      aout(self) << "cell #" << x.id() << " -> " << y << endl;
    });
}

void multiplexed_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom_v).then([=](int32_t y) {
      aout(self) << "cell #" << x.id() << " -> " << y << endl;
    });
}

void blocking_testee(blocking_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom_v)
      .receive(
        [&](int32_t y) {
          aout(self) << "cell #" << x.id() << " -> " << y << endl;
        },
        [&](error& err) {
          aout(self) << "cell #" << x.id() << " -> " << to_string(err) << endl;
        });
}
// --(rst-cell-end)--

void caf_main(actor_system& system) {
  vector<cell> cells;
  // --(rst-spawn-begin)--
  for (auto i = 0; i < 5; ++i)
    cells.emplace_back(system.spawn(cell_impl, i * i));
  // --(rst-spawn-end)--
  scoped_actor self{system};
  aout(self) << "waiting_testee" << endl;
  auto x1 = self->spawn(waiting_testee, cells);
  self->wait_for(x1);
  aout(self) << "multiplexed_testee" << endl;
  auto x2 = self->spawn(multiplexed_testee, cells);
  self->wait_for(x2);
  aout(self) << "blocking_testee" << endl;
  system.spawn(blocking_testee, cells);
}

CAF_MAIN()
