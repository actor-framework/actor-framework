// Non-interactive example to showcase the `iota` generator.

#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>

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
  auto n = get_or(cfg, "num-values", default_num_values);
  sys.spawn([n](caf::event_based_actor* self) {
    self
      // Get an observable factory.
      ->make_observable()
      // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
      .iota(1)
      // Only take the requested number of items from the infinite sequence.
      .take(n)
      // Print each integer.
      .for_each([](int x) { std::cout << x << std::endl; });
  });
}
// --(rst-main-end)--

} // namespace

CAF_MAIN()
