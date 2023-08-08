// This example shows how to use caf::after

#include "caf/all.hpp"

#include <chrono>
#include <iostream>
#include <random>

using std::cout;
using std::endl;

using namespace caf;

// Sends a random number of printable characters to buddy and exits
void generator(event_based_actor* self, actor buddy) {
  std::random_device rd;
  std::minstd_rand gen{rd()};
  const auto count = std::uniform_int_distribution<>{20, 100}(gen);
  std::uniform_int_distribution<> dis{33, 126};
  for (auto i = 0; i < count; i++) {
    self->send(buddy, static_cast<char>(dis(gen)));
  }
}

// Collects the incoming characters until either the awaited_size of characters
// is received, or no new characters arrive for 100ms
behavior collector(stateful_actor<std::string>* self, size_t awaited_size) {
  using namespace std::chrono_literals;
  self->state.reserve(awaited_size);
  return {
    [=](char c) {
      self->state.push_back(c);
      if (self->state.size() == awaited_size) {
        cout << "Received message length: " << self->state.size() << endl
             << "Message content: " << self->state << endl;
        self->quit();
      }
    },
    // trigger if we dont receive a message for 100ms
    caf::after(100ms) >>
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
  auto col = system.spawn(collector, 60);
  system.spawn(generator, col);
}

CAF_MAIN()
