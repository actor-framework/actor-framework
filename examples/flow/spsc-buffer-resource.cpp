// Non-interactive example to illustrate how to connect flows over an
// asynchronous SPSC (Single Producer Single Consumer) buffer manually. Usually,
// CAF generates the SPSC buffers implicitly.

#include "caf/actor_system.hpp"
#include "caf/async/spsc_buffer.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>

namespace {

constexpr size_t default_num_values = 100;

// --(rst-source-begin)--
// Simple source for generating a stream of integers from 1 to n.
void source(caf::event_based_actor* self,
            caf::async::producer_resource<int> out, size_t n) {
  self
    // Get an observable factory.
    ->make_observable()
    // Produce an integer sequence starting at 1, i.e., 1, 2, 3, ...
    .iota(1)
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
    .for_each([](int x) { std::cout << x << std::endl; });
}
// --(rst-sink-end)--

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
  auto [snk_res, src_res] = caf::async::make_spsc_buffer_resource<int>();
  auto n = get_or(cfg, "num-values", default_num_values);
  sys.spawn(sink, std::move(snk_res));
  sys.spawn(source, std::move(src_res), n);
}
// --(rst-main-end)--

} // namespace

CAF_MAIN()
