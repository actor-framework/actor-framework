// Non-interactive example to showcase `observe_on`.

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

// --(rst-main-begin)--
void caf_main(caf::actor_system& sys, const config& cfg) {
  // Create two actors without actually running them yet.
  using actor_t = caf::event_based_actor;
  auto [src, launch_src] = sys.spawn_inactive<actor_t>();
  auto [snk, launch_snk] = sys.spawn_inactive<actor_t>();
  // Define our data flow: generate data on `src` and print it on `snk`.
  src
    // Get an observable factory.
    ->make_observable()
    // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
    .from_callable([i = 0]() mutable { return ++i; })
    // Only take the requested number of items from the infinite sequence.
    .take(cfg.n)
    // Switch to `snk` for further processing.
    .observe_on(snk)
    // Print each integer.
    .for_each([](int x) { std::cout << x << '\n'; });
  // Allow the actors to run. After this point, we may no longer dereference
  // the `src` and `snk` pointers! Calling these manually is optional. When
  // removing these two lines, CAF automatically launches the actors at scope
  // exit.
  launch_src();
  launch_snk();
}
// --(rst-main-end)--

} // namespace

CAF_MAIN()
