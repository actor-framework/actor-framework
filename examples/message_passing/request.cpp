// This example illustrates semantics of request().{then|await|receive}.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/scoped_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

#include <chrono>
#include <cstdint>
#include <vector>

using std::endl;
using std::vector;

using namespace caf;
using namespace std::literals;

// --(rst-cell-begin)--
struct cell_trait {
  using signatures
    = type_list<result<void>(put_atom, int32_t), // 'put' writes to the cell
                result<int32_t>(get_atom)>;      // 'get 'reads from the cell
};
using cell = typed_actor<cell_trait>;

struct cell_state {
  static constexpr inline const char* name = "cell";

  cell::pointer self;

  int32_t value;

  cell_state(cell::pointer ptr, int32_t val) : self(ptr), value(val) {
    // nop
  }

  cell::behavior_type make_behavior() {
    return {
      [this](put_atom, int32_t val) { value = val; },
      [this](get_atom) { return value; },
    };
  }
};
// --(rst-cell-end)--

// --(rst-testees-begin)--
void waiting_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->mail(get_atom_v).request(x, 1s).await([self, x](int32_t y) {
      self->println("cell #{} -> {}", x.id(), y);
    });
}

void multiplexed_testee(event_based_actor* self, vector<cell> cells) {
  for (auto& x : cells)
    self->mail(get_atom_v).request(x, 1s).then([self, x](int32_t y) {
      self->println("cell #{} -> {}", x.id(), y);
    });
}

void blocking_testee(scoped_actor& self, vector<cell> cells) {
  for (auto& x : cells)
    self->mail(get_atom_v)
      .request(x, 1s)
      .receive([&](int32_t y) { self->println("cell #{} -> {}", x.id(), y); },
               [&](error& err) {
                 self->println("cell #{} -> {}", x.id(), err);
               });
}
// --(rst-testees-end)--

// --(rst-main-begin)--
void caf_main(actor_system& sys) {
  vector<cell> cells;
  for (int32_t i = 0; i < 5; ++i)
    cells.emplace_back(sys.spawn(actor_from_state<cell_state>, i * i));
  scoped_actor self{sys};
  self->println("spawn waiting testee");
  auto x1 = self->spawn(waiting_testee, cells);
  self->wait_for(x1);
  self->println("spawn multiplexed testee");
  auto x2 = self->spawn(multiplexed_testee, cells);
  self->wait_for(x2);
  self->println("run blocking testee");
  blocking_testee(self, cells);
}
// --(rst-main-end)--

CAF_MAIN()
