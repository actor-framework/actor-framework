// Non-interactive example to illustrate how to connect flows over an
// asynchronous SPSC (Single Producer Single Consumer) buffer.

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>

namespace {

// --(rst-source-begin)--
// Simple source for generating a stream of integers from 1 to n.
void source(caf::event_based_actor* self,
            caf::async::producer_resource<int> out, size_t n) {
  self
    // Get an observable factory.
    ->make_observable()
    // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
    .from_callable([i = 0]() mutable { return ++i; })
    // Only take the requested number of items from the infinite sequence.
    .take(n)
    // Subscribe the resource to the sequence, thereby starting the stream.
    .subscribe(out);
}
// --(rst-source-end)--

// --(rst-sink-begin)--
// Simple sink for consuming a stream of integers, printing it to stdout.
void sink(caf::event_based_actor* self, caf::async::consumer_resource<int> in) {
  self
    // Get an observable factory.
    ->make_observable()
    // Lift the input to an observable flow.
    .from_resource(std::move(in))
    // Print each integer.
    .for_each([](int x) { std::cout << x << '\n'; });
}
// --(rst-sink-end)--

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add(n, "num-values,n", "number of values produced by the source");
  }

  size_t n = 100;
};

// --(rst-main-begin)--
void caf_main(caf::actor_system& sys, const config& cfg) {
  auto [snk_res, src_res] = caf::async::make_spsc_buffer_resource<int>();
  sys.spawn(sink, std::move(snk_res));
  sys.spawn(source, std::move(src_res), cfg.n);
}
// --(rst-main-end)--

} // namespace

CAF_MAIN()
