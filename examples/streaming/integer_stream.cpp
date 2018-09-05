/******************************************************************************
 *     Basic, non-interactive streaming example for processing integers.      *
 ******************************************************************************/

// Manual refs: lines 17-52, 54-87, 89-120, 133-139 (Streaming)

#include <iostream>
#include <vector>

#include "caf/all.hpp"

using std::endl;
using namespace caf;

namespace {

// Simple source for generating a stream of integers from [0, n).
behavior int_source(event_based_actor* self) {
  return {
    [=](open_atom, int n) {
      // Produce at least one value.
      if (n <= 0)
        n = 1;
      // Create a stream manager for implementing a stream source. The
      // streaming logic requires three functions: initializer, generator, and
      // predicate.
      return self->make_source(
        // Initializer. The type of the first argument (state) is freely
        // chosen. If no state is required, `caf::unit_t` can be used here.
        [](int& x) {
          x = 0;
        },
        // Generator. This function is called by CAF to produce new stream
        // elements for downstream actors. The `x` argument is our state again
        // (with our freely chosen type). The second argument `out` points to
        // the output buffer. The template argument (here: int) determines what
        // elements downstream actors receive in this stream. Finally, `num` is
        // a hint from CAF how many elements we should ideally insert into
        // `out`. We can always insert fewer or more items.
        [n](int& x, downstream<int>& out, size_t num) {
          auto max_x = std::min(x + static_cast<int>(num), n);
          for (; x < max_x; ++x)
            out.push(x);
        },
        // Predicate. This function tells CAF when we reached the end.
        [n](const int& x) {
          return x == n;
        }
      );
    }
  };
}

// Simple stage that only selects even numbers.
behavior int_selector(event_based_actor* self) {
  return {
    [=](stream<int> in) {
      // Create a stream manager for implementing a stream stage. Similar to
      // `make_source`, we need three functions: initialzer, processor, and
      // finalizer.
      return self->make_stage(
        // Our input source.
        in,
        // Initializer. Here, we don't need any state and simply use unit_t.
        [](unit_t&) {
          // nop
        },
        // Processor. This function takes individual input elements as `val`
        // and forwards even integers to `out`.
        [](unit_t&, downstream<int>& out, int val) {
          if (val % 2 == 0)
            out.push(val);
        },
        // Finalizer. Allows us to run cleanup code once the stream terminates.
        [=](unit_t&, const error& err) {
          if (err) {
            aout(self) << "int_selector aborted with error: " << err
                       << std::endl;
          } else {
            aout(self) << "int_selector finalized" << std::endl;
          }
          // else: regular stream shutdown
        }
      );
    }
  };
}

behavior int_sink(event_based_actor* self) {
  return {
    [=](stream<int> in) {
      // Create a stream manager for implementing a stream sink. Once more, we
      // have to provide three functions: Initializer, Consumer, Finalizer.
      return self->make_sink(
        // Our input source.
        in,
        // Initializer. Here, we store all values we receive. Note that streams
        // are potentially unbound, so this is usually a bad idea outside small
        // examples like this one.
        [](std::vector<int>&) {
          // nop
        },
        // Consumer. Takes individual input elements as `val` and stores them
        // in our history.
        [](std::vector<int>& xs, int val) {
          xs.emplace_back(val);
        },
        // Finalizer. Allows us to run cleanup code once the stream terminates.
        [=](std::vector<int>& xs, const error& err) {
          if (err) {
            aout(self) << "int_sink aborted with error: " << err << std::endl;
          } else {
            aout(self) << "int_sink finalized after receiving: " << xs
                       << std::endl;
          }
        }
      );
    }
  };
}

struct config : actor_system_config {
  config() {
    opt_group{custom_options_, "global"}
    .add(with_stage, "with-stage,s", "use a stage for filtering odd numbers")
    .add(n, "num-values,n", "number of values produced by the source");
  }

  bool with_stage = false;
  int n = 100;
};

void caf_main(actor_system& sys, const config& cfg) {
  auto src = sys.spawn(int_source);
  auto snk = sys.spawn(int_sink);
  auto pipeline = cfg.with_stage ? snk * sys.spawn(int_selector) * src
                                 : snk * src;
  anon_send(pipeline, open_atom::value, cfg.n);
}

} // namespace <anonymous>

CAF_MAIN()
