// This example illustrates how to use a response promise to delay responding to
// an incoming message until a later point in time.

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/response_promise.hpp"
#include "caf/typed_event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

// --(rst-promise-begin)--
struct adder_trait {
  using signatures
    = caf::type_list<result<int32_t>(add_atom, int32_t, int32_t)>;
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
    [=](add_atom, int32_t y, int32_t z) {
      auto rp = self->make_response_promise<int32_t>();
      self->mail(add_atom_v, y, z)
        .request(worker, infinite)
        .then([rp](int32_t result) mutable { rp.deliver(result); },
              [rp](error& err) mutable { rp.deliver(std::move(err)); });
      return rp;
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
// --(rst-promise-end)--

CAF_MAIN()
