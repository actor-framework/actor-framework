#include <random>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include "caf/all.hpp"

using namespace caf;
using std::endl;

void caf_main(actor_system& system) {
  for (int i = 1; i <= 50; ++i) {
    system.spawn([i](blocking_actor* self) {
      aout(self) << "Hi there! This is actor nr. "
                 << i << "!" << endl;
      std::random_device rd;
      std::default_random_engine re(rd());
      std::chrono::milliseconds tout{re() % 10};
      self->delayed_send(self, tout, 42);
      self->receive(
        [i, self](int) {
          aout(self) << "Actor nr. "
                     << i << " says goodbye!" << endl;
        }
      );
    });
  }
}

CAF_MAIN()
