// Non-interactive example to showcase `from_callable`.

#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>

namespace {

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add(n, "num-values,n", "number of values produced by the source");
  }

  size_t n = 10;
};

// --(rst-from-callable-begin)--
void caf_main(caf::actor_system& sys, const config& cfg) {
  sys.spawn([n = cfg.n](caf::event_based_actor* self) {
    self
      // Get an observable factory.
      ->make_observable()
      // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
      .from_callable([i = 0]() mutable { return ++i; })
      // Only take the requested number of items from the infinite sequence.
      .take(n)
      // Print each integer.
      .for_each([](int x) { std::cout << x << '\n'; });
  });
}
// --(rst-from-callable-end)--

} // namespace

CAF_MAIN()
