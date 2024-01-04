// This example illustrates how to use the `delegate` function to forward
// messages to another actor.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

// --(rst-delegate-begin)--
using adder_actor = typed_actor<result<int32_t>(add_atom, int32_t, int32_t)>;

adder_actor::behavior_type worker_impl() {
  return {
    [](add_atom, int32_t x, int32_t y) { return x + y; },
  };
}
adder_actor::behavior_type server_impl(adder_actor::pointer self,
                                       adder_actor worker) {
  return {
    [=](add_atom add, int32_t x, int32_t y) {
      return self->delegate(worker, add, x, y);
    },
  };
}

void client_impl(event_based_actor* self, adder_actor adder, int32_t x,
                 int32_t y) {
  self->request(adder, 10s, add_atom_v, x, y).then([=](int32_t result) {
    aout(self) << x << " + " << y << " = " << result << std::endl;
  });
}

void caf_main(actor_system& sys) {
  auto worker = sys.spawn(worker_impl);
  auto server = sys.spawn(server_impl, sys.spawn(worker_impl));
  sys.spawn(client_impl, server, 1, 2);
}
// --(rst-delegate-end)--

CAF_MAIN()
