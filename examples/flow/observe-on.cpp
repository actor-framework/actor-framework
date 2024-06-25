// Non-interactive example to showcase `observe_on`.

#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

namespace {

constexpr size_t default_num_values = 10;

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<size_t>("num-values,n", "number of values produced by the source");
  }

  caf::settings dump_content() const override {
    auto result = actor_system_config::dump_content();
    caf::put_missing(result, "num-values", default_num_values);
    return result;
  }
};

// --(rst-main-begin)--
void caf_main(caf::actor_system& sys, const config& cfg) {
  // Create two actors without actually running them yet.
  auto n = get_or(cfg, "num-values", default_num_values);
  auto [src, launch_src] = sys.spawn_inactive();
  auto [snk, launch_snk] = sys.spawn_inactive();
  // Define our data flow: generate data on `src` and print it on `snk`.
  src
    // Get an observable factory.
    ->make_observable()
    // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
    .iota(1)
    // Only take the requested number of items from the infinite sequence.
    .take(n)
    // Switch to `snk` for further processing.
    .observe_on(snk)
    // Print each integer.
    .for_each([snk = snk](int x) { snk->println("{}", x); });
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
