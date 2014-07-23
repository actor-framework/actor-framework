/******************************************************************************\
 * This example is an implementation of the classical Dining Philosophers     *
 * exercise using only libcaf's event-based actor implementation.             *
\ ******************************************************************************/

#include <map>
#include <vector>
#include <chrono>
#include <sstream>
#include <iostream>

#include "caf/all.hpp"

using std::chrono::seconds;

using namespace std;
using namespace caf;

namespace {

// either taken by a philosopher or available
void chopstick(event_based_actor* self) {
  self->become(
    on(atom("take"), arg_match) >> [=](const actor& philos) {
      // tell philosopher it took this chopstick
      self->send(philos, atom("taken"), self);
      // await 'put' message and reject other 'take' messages
      self->become(
        // allows us to return to the previous behavior
        keep_behavior,
        on(atom("take"), arg_match) >> [=](const actor& other) {
          self->send(other, atom("busy"), self);
        },
        on(atom("put"), philos) >> [=] {
          // return to previous behaivor, i.e., await next 'take'
          self->unbecome();
        }
      );
    }
  );
}

/* See: http://www.dalnefre.com/wp/2010/08/dining-philosophers-in-humus/
 *
 *
 *                +-------------+  {(busy|taken), Y}
 *      /-------->|  thinking   |<------------------\
 *      |         +-------------+                   |
 *      |                |                          |
 *      |                | {eat}                    |
 *      |                |                          |
 *      |                V                          |
 *      |         +-------------+ {busy, X}  +-------------+
 *      |         |   hungry    |----------->|   denied    |
 *      |         +-------------+            +-------------+
 *      |                |
 *      |                | {taken, X}
 *      |                |
 *      |                V
 *      |         +-------------+
 *      |         | wait_for(Y) |
 *      |         +-------------+
 *      |           |    |
 *      | {busy, Y} |    | {taken, Y}
 *      \-----------/    |
 *      |                V
 *      | {think} +-------------+
 *      \---------|   eating    |
 *                +-------------+
 *
 *
 * [ X = left  => Y = right ]
 * [ X = right => Y = left  ]
 */

class philosopher : public event_based_actor {
 public:
  philosopher(const std::string& n, const actor& l, const actor& r)
      : name(n), left(l), right(r) {
    // a philosopher that receives {eat} stops thinking and becomes hungry
    thinking = (
      on(atom("eat")) >> [=] {
        become(hungry);
        send(left, atom("take"), this);
        send(right, atom("take"), this);
      }
    );
    // wait for the first answer of a chopstick
    hungry = (
      on(atom("taken"), left) >> [=] {
        become(waiting_for(right));
      },
      on(atom("taken"), right) >> [=] {
        become(waiting_for(left));
      },
      on<atom("busy"), actor>() >> [=] {
        become(denied);
      }
    );
    // philosopher was not able to obtain the first chopstick
    denied = (
      on(atom("taken"), arg_match) >> [=](const actor& ptr) {
        send(ptr, atom("put"), this);
        send(this, atom("eat"));
        become(thinking);
      },
      on<atom("busy"), actor>() >> [=] {
        send(this, atom("eat"));
        become(thinking);
      }
    );
    // philosopher obtained both chopstick and eats (for five seconds)
    eating = (
      on(atom("think")) >> [=] {
        send(left, atom("put"), this);
        send(right, atom("put"), this);
        delayed_send(this, seconds(5), atom("eat"));
        aout(this) << name << " puts down his chopsticks and starts to think\n";
        become(thinking);
      }
    );
  }

 protected:
  behavior make_behavior() override {
    // start thinking
    send(this, atom("think"));
    // philosophers start to think after receiving {think}
    return (
      on(atom("think")) >> [=] {
        aout(this) << name << " starts to think\n";
        delayed_send(this, seconds(5), atom("eat"));
        become(thinking);
      }
    );
  }

 private:
  // wait for second chopstick
  behavior waiting_for(const actor& what) {
    return {
      on(atom("taken"), what) >> [=] {
        aout(this) << name
                   << " has picked up chopsticks with IDs "
                   << left->id() << " and " << right->id()
                   << " and starts to eat\n";
        // eat some time
        delayed_send(this, seconds(5), atom("think"));
        become(eating);
      },
      on(atom("busy"), what) >> [=] {
        send((what == left) ? right : left, atom("put"), this);
        send(this, atom("eat"));
        become(thinking);
      }
    };
  }

  std::string name;   // the name of this philosopher
  actor       left;   // left chopstick
  actor       right;  // right chopstick
  behavior    thinking;
  behavior    hungry; // tries to take chopsticks
  behavior    denied; // could not get chopsticks
  behavior    eating; // wait for some time, then go thinking again
};

void dining_philosophers() {
  scoped_actor self;
  // create five chopsticks
  aout(self) << "chopstick ids are:";
  std::vector<actor> chopsticks;
  for (size_t i = 0; i < 5; ++i) {
    chopsticks.push_back(spawn(chopstick));
    aout(self) << " " << chopsticks.back()->id();
  }
  aout(self) << endl;
  // spawn five philosophers
  std::vector<std::string> names {"Plato", "Hume", "Kant",
                                   "Nietzsche", "Descartes"};
  for (size_t i = 0; i < 5; ++i) {
    spawn<philosopher>(names[i], chopsticks[i], chopsticks[(i + 1) % 5]);
  }
}

} // namespace <anonymous>

int main(int, char**) {
  dining_philosophers();
  // real philosophers are never done
  await_all_actors_done();
  shutdown();
  return 0;
}
