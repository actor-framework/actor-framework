// Illustrates how to use response promises.

#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;

using namespace caf;

// --(rst-promises-begin)--
using adder
  = typed_actor<replies_to<add_atom, int32_t, int32_t>::with<int32_t>>;

adder::behavior_type worker() {
  return {
    [](add_atom, int32_t a, int32_t b) { return a + b; },
  };
}

adder::behavior_type calculator_master(adder::pointer self) {
  auto w = self->spawn(worker);
  return {
    [=](add_atom x, int32_t y, int32_t z) -> result<int32_t> {
      auto rp = self->make_response_promise<int32_t>();
      self->request(w, infinite, x, y, z).then([=](int32_t result) mutable {
        rp.deliver(result);
      });
      return rp;
    },
  };
}
// --(rst-promises-end)--

void caf_main(actor_system& system) {
  auto f = make_function_view(system.spawn(calculator_master));
  cout << "12 + 13 = " << f(add_atom_v, 12, 13) << endl;
}

CAF_MAIN()
