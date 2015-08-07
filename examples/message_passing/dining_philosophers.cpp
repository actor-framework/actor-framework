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

using std::cout;
using std::cerr;
using std::endl;
using std::chrono::seconds;

using namespace caf;

namespace {

// atoms for chopstick interface
using put_atom = atom_constant<atom("put")>;
using take_atom = atom_constant<atom("take")>;
using busy_atom = atom_constant<atom("busy")>;
using taken_atom = atom_constant<atom("taken")>;

// atoms for philosopher interface
using eat_atom = atom_constant<atom("eat")>;
using think_atom = atom_constant<atom("think")>;

// a chopstick
using chopstick = typed_actor<replies_to<take_atom>
                              ::with_either<taken_atom>
                              ::or_else<busy_atom>,
                              reacts_to<put_atom>>;

chopstick::behavior_type taken_chopstick(chopstick::pointer self, actor_addr);

// either taken by a philosopher or available
chopstick::behavior_type available_chopstick(chopstick::pointer self) {
  return {
    [=](take_atom) {
      self->become(taken_chopstick(self, self->current_sender()));
      return taken_atom::value;
    },
    [](put_atom) {
      cerr << "chopstick received unexpected 'put'" << endl;
    }
  };
}

chopstick::behavior_type taken_chopstick(chopstick::pointer self,
                                         actor_addr user) {
  return {
    [](take_atom) {
      return busy_atom::value;
    },
    [=](put_atom) {
      if (self->current_sender() == user) {
        self->become(available_chopstick(self));
      }
    }
  };
}

/* Based on: http://www.dalnefre.com/wp/2010/08/dining-philosophers-in-humus/
 *
 *
 *                +-------------+     {busy|taken}
 *      /-------->|  thinking   |<------------------\
 *      |         +-------------+                   |
 *      |                |                          |
 *      |                | {eat}                    |
 *      |                |                          |
 *      |                V                          |
 *      |         +-------------+  {busy}    +-------------+
 *      |         |   hungry    |----------->|   denied    |
 *      |         +-------------+            +-------------+
 *      |                |
 *      |                | {taken}
 *      |                |
 *      |                V
 *      |         +-------------+
 *      |         |   granted   |
 *      |         +-------------+
 *      |           |    |
 *      |  {busy}   |    | {taken}
 *      \-----------/    |
 *      |                V
 *      | {think} +-------------+
 *      \---------|   eating    |
 *                +-------------+
 */

class philosopher : public event_based_actor {
public:
  philosopher(const std::string& n, const chopstick& l, const chopstick& r)
      : name(n),
        left(l),
        right(r) {
    // a philosopher that receives {eat} stops thinking and becomes hungry
    thinking.assign(
      [=](eat_atom) {
        become(hungry);
        send(left, take_atom::value);
        send(right, take_atom::value);
      }
    );
    // wait for the first answer of a chopstick
    hungry.assign(
      [=](taken_atom) {
        become(granted);
      },
      [=](busy_atom) {
        become(denied);
      }
    );
    // philosopher was able to obtain the first chopstick
    granted.assign(
      [=](taken_atom) {
        aout(this) << name
                   << " has picked up chopsticks with IDs "
                   << left->id() << " and " << right->id()
                   << " and starts to eat\n";
        // eat some time
        delayed_send(this, seconds(5), think_atom::value);
        become(eating);
      },
      [=](busy_atom) {
        send(current_sender() == left ? right : left, put_atom::value);
        send(this, eat_atom::value);
        become(thinking);
      }
    );
    // philosopher was *not* able to obtain the first chopstick
    denied.assign(
      [=](taken_atom) {
        send(current_sender() == left ? left : right, put_atom::value);
        send(this, eat_atom::value);
        become(thinking);
      },
      [=](busy_atom) {
        send(this, eat_atom::value);
        become(thinking);
      }
    );
    // philosopher obtained both chopstick and eats (for five seconds)
    eating.assign(
      [=](think_atom) {
        send(left, put_atom::value);
        send(right, put_atom::value);
        delayed_send(this, seconds(5), eat_atom::value);
        aout(this) << name << " puts down his chopsticks and starts to think\n";
        become(thinking);
      }
    );
  }

protected:
  behavior make_behavior() override {
    // start thinking
    send(this, think_atom::value);
    // philosophers start to think after receiving {think}
    return (
      [=](think_atom) {
        aout(this) << name << " starts to think\n";
        delayed_send(this, seconds(5), eat_atom::value);
        become(thinking);
      }
    );
  }

private:
  std::string name;     // the name of this philosopher
  chopstick   left;     // left chopstick
  chopstick   right;    // right chopstick
  behavior    thinking; // initial behavior
  behavior    hungry;   // tries to take chopsticks
  behavior    granted;  // has one chopstick and waits for the second one
  behavior    denied;   // could not get first chopsticks
  behavior    eating;   // waits for some time, then go thinking again
};

void dining_philosophers() {
  scoped_actor self;
  // create five chopsticks
  aout(self) << "chopstick ids are:";
  std::vector<chopstick> chopsticks;
  for (size_t i = 0; i < 5; ++i) {
    chopsticks.push_back(spawn(available_chopstick));
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
}
