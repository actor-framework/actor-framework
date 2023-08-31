// This example illustrates semantics of request().{then|await|receive}.

#include "caf/all.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using std::endl;
using std::vector;
using std::chrono::seconds;
using namespace caf;

// --(rst-cell-begin)--
using cell
  = typed_actor<result<void>(put_atom, int32_t), // 'put' writes to the cell
                result<int32_t>(get_atom)>;      // 'get 'reads from the cell

struct cell_state {
  static constexpr inline const char* name = "cell";

  cell::pointer self;

  int32_t value;

  cell_state(cell::pointer ptr, int32_t val) : self(ptr), value(val) {
    // nop
  }

  cell_state(const cell_state&) = delete;

  cell_state& operator=(const cell_state&) = delete;

  cell::behavior_type make_behavior() {
    return {
      [this](put_atom, int32_t val) { value = val; },
      [this](get_atom) { return value; },
    };
  }
};

using cell_impl = cell::stateful_impl<cell_state>;
// --(rst-cell-end)--

// --(rst-testees-begin)--
void waiting_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom_v).await([self, x](int32_t y) {
      aout(self) << "cell #" << x.id() << " -> " << y << endl;
    });
}

void multiplexed_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->request(x, seconds(1), get_atom_v).then([self, x](int32_t y) {
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
// --(rst-testees-end)--

// --(rst-main-begin)--
void caf_main(actor_system& system) {
  vector<cell> cells;
  for (int32_t i = 0; i < 5; ++i)
    cells.emplace_back(system.spawn<cell_impl>(i * i));
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
// --(rst-main-end)--

CAF_MAIN()
