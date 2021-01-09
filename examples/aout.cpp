#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"

using namespace caf;

behavior printer(event_based_actor* self, int32_t num, int32_t delay) {
  aout(self) << "Hi there! This is actor nr. " << num << "!" << std::endl;
  std::chrono::milliseconds timeout{delay};
  self->delayed_send(self, timeout, timeout_atom_v);
  return {
    [=](timeout_atom) {
      aout(self) << "Actor nr. " << num << " says goodbye after waiting for "
                 << delay << "ms!" << std::endl;
    },
  };
}

void caf_main(actor_system& sys) {
  std::random_device rd;
  std::minstd_rand re(rd());
  std::uniform_int_distribution<int32_t> dis{1, 99};
  for (int32_t i = 1; i <= 50; ++i)
    sys.spawn(printer, i, dis(re));
}

CAF_MAIN()
