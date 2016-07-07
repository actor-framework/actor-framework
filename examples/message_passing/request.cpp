/******************************************************************************\
 * Illustrates semantics of request().{then|await|receive}.                   *
\******************************************************************************/

// This file is partially included in the manual, do not modify
// without updating the references in the *.tex files!
// Manual references: lines 20-37, 39-51, 53-64, 67-69 (MessagePassing.tex)

#include <vector>
#include <chrono>
#include <iostream>

#include "caf/all.hpp"

using std::endl;
using std::vector;
using std::chrono::seconds;
using namespace caf;

using cell = typed_actor<reacts_to<put_atom, int>,
                         replies_to<get_atom>::with<int>>;

struct cell_state {
  int value = 0;
};

cell::behavior_type cell_impl(cell::stateful_pointer<cell_state> self, int x0) {
  self->state.value = x0;
  return {
    [=](put_atom, int val) {
      self->state.value = val;
    },
    [=](get_atom) {
      return self->state.value;
    }
  };
}

void waiting_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom::value).await([=](int y) {
      aout(self) << "cell #" << x.id() << " -> " << y << endl;
    });
}

void multiplexed_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom::value).then([=](int y) {
      aout(self) << "cell #" << x.id() << " -> " << y << endl;
    });
}

void blocking_testee(blocking_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom::value).receive(
      [&](int y) {
        aout(self) << "cell #" << x.id() << " -> " << y << endl;
      },
      [&](error& err) {
        aout(self) << "cell #" << x.id()
                   << " -> " << self->system().render(err) << endl;
      }
    );
}

void caf_main(actor_system& system) {
  vector<cell> cells;
  for (auto i = 0; i < 5; ++i)
    cells.emplace_back(system.spawn(cell_impl, i * i));
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
