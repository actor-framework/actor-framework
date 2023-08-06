// This example shows how to use caf::after

#include "caf/all.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
using std::cout;
using std::endl;

using namespace caf;

// Sends random caracters to buddy, and then waits for a letter back
behavior noisy_buddy(event_based_actor* self, actor buddy) {
  using namespace std::chrono_literals;
  std::random_device rd;
  std::minstd_rand gen{rd()};
  const auto count = std::uniform_int_distribution<>(20, 100)(gen);
  std::uniform_int_distribution<> dis(33, 126);
  for (auto i = 0; i < count; i++) {
    self->send(buddy, static_cast<char>(dis(gen)));
  }
  return {[self](std::string letter) {
    cout << "Received a letter:" << endl << letter << endl;
    self->quit();
  }};
}

struct aggregator_state {
  std::string letter;
  caf::actor dest;
};

// Aggregates incoming characters and stores the sender, replies with the
// reversed string when inactive for 100ms
behavior aggregator(stateful_actor<aggregator_state>* self) {
  using namespace std::chrono_literals;
  return {
    [=](char c) mutable {
      self->state.dest = caf::actor_cast<caf::actor>(self->current_sender());
      self->state.letter.push_back(c);
    },
    // trigger if we dont receive a message for 100ms
    caf::after(100ms) >>
      [=] {
        std::reverse(self->state.letter.begin(), self->state.letter.end());
        self->send(self->state.dest, self->state.letter);
        cout << "bye" << endl;
        self->quit();
      }};
}

void caf_main(actor_system& system) {
  auto agg = system.spawn(aggregator);
  system.spawn(noisy_buddy, agg);
}

CAF_MAIN()
