#include "caf/all.hpp"

using namespace caf;

// --(rst-promise-begin)--
using adder_actor = typed_actor<result<int32_t>(add_atom, int32_t, int32_t)>;

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
      self->request(worker, infinite, add_atom_v, y, z)
        .then([rp](int32_t result) mutable { rp.deliver(result); },
              [rp](error& err) mutable { rp.deliver(std::move(err)); });
      return rp;
    },
  };
}

void client_impl(event_based_actor* self, adder_actor adder, int32_t x,
                 int32_t y) {
  using namespace std::literals::chrono_literals;
  self->request(adder, 10s, add_atom_v, x, y).then([=](int32_t result) {
    aout(self) << x << " + " << y << " = " << result << std::endl;
  });
}

void caf_main(actor_system& sys) {
  auto worker = sys.spawn(worker_impl);
  auto server = sys.spawn(server_impl, sys.spawn(worker_impl));
  sys.spawn(client_impl, server, 1, 2);
}
// --(rst-promise-end)--

CAF_MAIN()
