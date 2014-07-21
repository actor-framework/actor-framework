/******************************************************************************\
 * This example illustrates how to use aout.                                  *
\ ******************************************************************************/

#include <chrono>
#include <cstdlib>
#include <iostream>
#include "caf/all.hpp"

using namespace caf;
using std::endl;

int main() {
  std::srand(static_cast<unsigned>(std::time(0)));
  for (int i = 1; i <= 50; ++i) {
    spawn<blocking_api>([i](blocking_actor* self) {
      aout(self) << "Hi there! This is actor nr. "
                 << i << "!" << endl;
      std::chrono::milliseconds tout{std::rand() % 1000};
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
