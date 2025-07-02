// This example shows how to use an idle timeout that triggers multiple times.

#include "caf/actor_from_state.hpp"
#include "caf/actor_system.hpp"
#include "caf/anon_mail.hpp"
#include "caf/caf_main.hpp"
#include "caf/chrono.hpp"
#include "caf/event_based_actor.hpp"

#include <chrono>
#include <string>
#include <string_view>

using namespace caf;
using namespace std::literals;

using system_clock = std::chrono::system_clock;

// Simply waits until 5 timeouts triggered and then quits.
struct testee_state {
  explicit testee_state(event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  behavior make_behavior() {
    using ms = std::chrono::milliseconds;
    // Trigger after 250ms of inactivity. Keep the actor alive even if no other
    // actor holds a reference to it and run the callback until the actor quits.
    self->set_idle_handler(500ms, strong_ref, repeat, [this] {
      ++num_timeouts;
      self->println("[{}] Timeout #{}!",
                    caf::chrono::to_string<ms>(system_clock::now()),
                    num_timeouts);
      if (num_timeouts == 5)
        self->quit();
    });
    // Return the behavior for the actor.
    return {
      [this](const std::string& str) {
        // When receiving a message, it cancels the current idle timeout and
        // sets a new one. Hence, the next idle timeout will happen 500ms after
        // receiving this message (unless another message arrives).
        self->println("[{}] Received: {}",
                      caf::chrono::to_string<ms>(system_clock::now()), str);
      },
    };
  }

  event_based_actor* self;
  int num_timeouts = 0;
};

void caf_main(actor_system& sys) {
  auto testee = sys.spawn(actor_from_state<testee_state>);
  anon_mail("Hello testee!").delay(800ms).send(testee);
}

CAF_MAIN()
