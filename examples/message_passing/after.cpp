// This example shows how to use caf::after

#include "caf/after.hpp"

#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/stateful_actor.hpp"

#include <chrono>
#include <iostream>
#include <random>
#include <string>

using std::cout;
using std::endl;

using namespace caf;

// Sends a random number of printable characters to buddy and exits
void generator(event_based_actor* self, actor sink) {
  std::random_device rd;
  std::minstd_rand gen{rd()};
  const auto count = std::uniform_int_distribution<>{20, 100}(gen);
  std::uniform_int_distribution<> dis{33, 126};
  for (auto i = 0; i < count; i++) {
    self->send(sink, static_cast<char>(dis(gen)));
  }
}

// Collects the incoming characters until no new characters arrive for 500ms.
// Prints every 60 characters.
behavior collector(stateful_actor<std::string>* self) {
  using namespace std::chrono_literals;
  return {
    [self](char c) {
      self->state.push_back(c);
      constexpr auto flush_threshold = 60;
      if (self->state.size() == flush_threshold) {
        cout << "Received message length: " << self->state.size() << endl
             << "Message content: " << self->state << endl;
        self->state.clear();
      }
    },
    caf::after(500ms) >>
      [self]() {
        cout << "Timeout reached!" << endl;
        if (!self->state.empty()) {
          cout << "Received message length: " << self->state.size() << endl
               << "Message content: " << self->state << endl;
        }
        self->quit();
      },
  };
}

void caf_main(actor_system& system) {
  auto col = system.spawn(collector);
  system.spawn(generator, col);
}

CAF_MAIN()
