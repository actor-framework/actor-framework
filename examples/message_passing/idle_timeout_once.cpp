// This example shows how to use an idle timeout that triggers only once.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/after.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

#include <chrono>
#include <random>
#include <string_view>
#include <vector>

using namespace caf;
using namespace std::literals;

constexpr size_t flush_threshold = 60;

// Sends a random number of printable characters to `sink` and then quits.
void generator(event_based_actor* self, actor sink) {
  std::random_device rd;
  std::minstd_rand gen{rd()};
  const auto count = std::uniform_int_distribution<>{20, 100}(gen);
  std::uniform_int_distribution<> dis{33, 126};
  for (auto i = 0; i < count; i++) {
    self->mail(static_cast<char>(dis(gen))).send(sink);
  }
}

// Collects the incoming characters until no new characters arrive for 500ms.
// Prints every 60 characters.
struct collector_state {
  explicit collector_state(event_based_actor* selfptr) : self(selfptr) {
    // nop
  }

  behavior make_behavior() {
    // Trigger after 500ms of inactivity. Keep the actor alive even if no other
    // actor holds a reference to it and run the callback only once.
    self->set_idle_handler(500ms, strong_ref, once, [this] {
      if (!buf.empty()) {
        aout(self)
          .println("Timeout reached!")
          .println("Received message length: {}", buf.size())
          .println("Message content: {}", str());
      } else {
        aout(self).println("Timeout reached with an empty buffer!");
      }
      self->quit();
    });
    // Return the behavior for the actor.
    return {
      [this](char c) {
        buf.push_back(c);
        if (buf.size() == flush_threshold) {
          aout(self)
            .println("Received message length: {}", buf.size())
            .println("Message content: {}", str());
          buf.clear();
        }
      },
    };
  }

  std::string_view str() {
    return {buf.data(), buf.size()};
  }

  event_based_actor* self;
  std::vector<char> buf;
};

void caf_main(actor_system& sys) {
  auto col = sys.spawn(actor_from_state<collector_state>);
  sys.spawn(generator, col);
}

CAF_MAIN()
