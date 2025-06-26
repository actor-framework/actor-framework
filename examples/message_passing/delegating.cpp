// This example illustrates how to use the `delegate` function to forward
// messages to another actor.

#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/typed_actor.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

// --(rst-delegate-begin)--
struct adder_trait {
  using signatures = type_list<result<int32_t>(add_atom, int32_t, int32_t)>;
};
using adder_actor = typed_actor<adder_trait>;

adder_actor::behavior_type worker_impl() {
  return {
    [](add_atom, int32_t x, int32_t y) { return x + y; },
  };
}
adder_actor::behavior_type server_impl(adder_actor::pointer self,
                                       adder_actor worker) {
  return {
    [self, worker](add_atom add, int32_t x, int32_t y) {
      return self->mail(add, x, y).delegate(worker);
    },
  };
}

void client_impl(event_based_actor* self, adder_actor adder, int32_t x,
                 int32_t y) {
  self->mail(add_atom_v, x, y).request(adder, 10s).then([=](int32_t result) {
    self->println("{} + {} = {}", x, y, result);
  });
}

void caf_main(actor_system& sys) {
  auto worker = sys.spawn(worker_impl);
  auto server = sys.spawn(server_impl, sys.spawn(worker_impl));
  sys.spawn(client_impl, server, 1, 2);
}
// --(rst-delegate-end)--

CAF_MAIN()
