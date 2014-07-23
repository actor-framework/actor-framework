/******************************************************************************\
 * This example illustrates how to use aout.                                  *
\ ******************************************************************************/

#include <random>
#include <chrono>
#include <cstdlib>
#include <iostream>

#include "caf/all.hpp"

using namespace caf;
using std::endl;

int main() {
  for (int i = 1; i <= 50; ++i) {
    spawn<blocking_api>([i](blocking_actor* self) {
      aout(self) << "Hi there! This is actor nr. "
                 << i << "!" << endl;
      std::random_device rd;
      std::default_random_engine re(rd());
      std::chrono::milliseconds tout{re() % 10};
      self->delayed_send(self, tout, atom("done"));
      self->receive(others() >> [i, self] {
        aout(self) << "Actor nr. "
                   << i << " says goodbye!" << endl;
      });
    });
  }
  // wait until all other actors we've spawned are done
  await_all_actors_done();
  // done
  shutdown();
  return 0;
}
