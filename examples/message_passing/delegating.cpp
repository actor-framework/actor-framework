#include <cstdint>
#include <iostream>

#include "caf/all.hpp"

using namespace caf;

// --(rst-delegate-begin)--
using calc = typed_actor<replies_to<add_atom, int32_t, int32_t>::with<int32_t>>;

void actor_a(event_based_actor* self, const calc& worker) {
  self->request(worker, std::chrono::seconds(10), add_atom_v, 1, 2)
    .then([=](int32_t result) { //
      aout(self) << "1 + 2 = " << result << std::endl;
    });
}

calc::behavior_type actor_b(calc::pointer self, const calc& worker) {
  return {
    [=](add_atom add, int32_t x, int32_t y) {
      return self->delegate(worker, add, x, y);
    },
  };
}

calc::behavior_type actor_c() {
  return {
    [](add_atom, int32_t x, int32_t y) { return x + y; },
  };
}
// --(rst-delegate-end)--

void caf_main(actor_system& system) {
  system.spawn(actor_a, system.spawn(actor_b, system.spawn(actor_c)));
}

CAF_MAIN()
