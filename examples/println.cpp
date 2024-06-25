#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"

#include <chrono>
#include <cstdlib>
#include <random>

using namespace caf;

constexpr int32_t num_actors = 50;

behavior printer(event_based_actor* self, int32_t num, int32_t delay) {
  self->println("Hi there! This is actor nr. {}!", num);
  auto timeout = std::chrono::milliseconds{delay};
  self->mail(timeout_atom_v).delay(timeout).send(self);
  return {
    [=](timeout_atom) {
      self->println("Actor nr. {} says goodbye after waiting for {}ms!", num,
                    delay);
    },
  };
}

void caf_main(actor_system& sys) {
  sys.println("Spawning {} actors...", num_actors);
  std::random_device rd;
  std::minstd_rand re{rd()};
  std::uniform_int_distribution<int32_t> dis{1, 99};
  for (int32_t i = 1; i <= 50; ++i)
    sys.spawn(printer, i, dis(re));
}

CAF_MAIN()
