// This example is an implementation of the classical Dining Philosophers
// exercise using only CAF's event-based actor implementation.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/scoped_actor.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

CAF_BEGIN_TYPE_ID_BLOCK(dining_philosophers, first_custom_type_id)

  CAF_ADD_ATOM(dining_philosophers, take_atom)
  CAF_ADD_ATOM(dining_philosophers, taken_atom)
  CAF_ADD_ATOM(dining_philosophers, eat_atom)
  CAF_ADD_ATOM(dining_philosophers, think_atom)

CAF_END_TYPE_ID_BLOCK(dining_philosophers)

using std::cerr;
using std::endl;

using namespace caf;
using namespace std::literals;

// a chopstick: either taken by a philosopher or available
using chopstick
  = typed_actor<result<taken_atom, bool>(take_atom), result<void>(put_atom)>;

struct chopstick_state {
  explicit chopstick_state(chopstick::pointer selfptr) : self(selfptr) {
    // nop
  }

  chopstick::behavior_type make_behavior() {
    return {
      [this](take_atom) -> result<taken_atom, bool> {
        self->become(keep_behavior, taken(self->current_sender()));
        return {taken_atom_v, true};
      },
      [](put_atom) { cerr << "chopstick received unexpected 'put'" << endl; },
    };
  }

  chopstick::behavior_type taken(const strong_actor_ptr& user) {
    return {
      [](take_atom) -> result<taken_atom, bool> {
        return {taken_atom_v, false};
      },
      [this, user](put_atom) {
        if (self->current_sender() == user)
          self->unbecome();
      },
    };
  }

  chopstick::pointer self;
};

// Based on: http://www.dalnefre.com/wp/2010/08/dining-philosophers-in-humus/
//
//
//                +-------------+     {busy|taken}
//      +-------->|  thinking   |<------------------+
//      |         +-------------+                   |
//      |                |                          |
//      |                | {eat}                    |
//      |                |                          |
//      |                V                          |
//      |         +-------------+  {busy}    +-------------+
//      |         |   hungry    |----------->|   denied    |
//      |         +-------------+            +-------------+
//      |                |
//      |                | {taken}
//      |                |
//      |                V
//      |         +-------------+
//      |         |   granted   |
//      |         +-------------+
//      |           |    |
//      |  {busy}   |    | {taken}
//      +-----------+    |
//      |                V
//      | {think} +-------------+
//      +---------|   eating    |
//                +-------------+

class philosopher_state {
public:
  philosopher_state(event_based_actor* selfptr, std::string n, chopstick l,
                    chopstick r)
    : self(selfptr),
      name(std::move(n)),
      left(std::move(l)),
      right(std::move(r)) {
    // we only accept one message per state and skip others in the meantime
    self->set_default_handler(skip);
    // a philosopher that receives {eat} stops thinking and becomes hungry
    thinking.assign([this](eat_atom) {
      self->become(hungry);
      self->send(left, take_atom_v);
      self->send(right, take_atom_v);
    });
    // wait for the first answer of a chopstick
    hungry.assign([this](taken_atom, bool result) {
      if (result)
        self->become(granted);
      else
        self->become(denied);
    });
    // philosopher was able to obtain the first chopstick
    granted.assign([this](taken_atom, bool result) {
      if (result) {
        aout(self) << name << " has picked up chopsticks with IDs "
                   << left->id() << " and " << right->id()
                   << " and starts to eat\n";
        // eat some time
        self->delayed_send(self, 5s, think_atom_v);
        self->become(eating);
      } else {
        self->send(self->current_sender() == left ? right : left, put_atom_v);
        self->send(self, eat_atom_v);
        self->become(thinking);
      }
    });
    // philosopher was *not* able to obtain the first chopstick
    denied.assign([this](taken_atom, bool result) {
      if (result)
        self->send(self->current_sender() == left ? left : right, put_atom_v);
      self->send(self, eat_atom_v);
      self->become(thinking);
    });
    // philosopher obtained both chopstick and eats (for five seconds)
    eating.assign([this](think_atom) {
      self->send(left, put_atom_v);
      self->send(right, put_atom_v);
      self->delayed_send(self, 5s, eat_atom_v);
      aout(self) << name << " puts down his chopsticks and starts to think\n";
      self->become(thinking);
    });
  }

  behavior make_behavior() {
    // start thinking
    self->send(self, think_atom_v);
    // philosophers start to think after receiving {think}
    return {
      [this](think_atom) {
        aout(self) << name << " starts to think\n";
        self->delayed_send(self, 5s, eat_atom_v);
        self->become(thinking);
      },
    };
  }

  event_based_actor* self; // handle to the actor itself
  std::string name;        // the name of this philosopher
  chopstick left;          // left chopstick
  chopstick right;         // right chopstick
  behavior thinking;       // initial behavior
  behavior hungry;         // tries to take chopsticks
  behavior granted;        // has one chopstick and waits for the second one
  behavior denied;         // could not get first chopsticks
  behavior eating;         // wait for some time, then go thinking again
};

void caf_main(actor_system& sys) {
  scoped_actor self{sys};
  // create five chopsticks
  aout(self) << "chopstick ids are:";
  auto chopsticks = std::vector<chopstick>{};
  for (size_t i = 0; i < 5; ++i) {
    chopsticks.push_back(self->spawn(actor_from_state<chopstick_state>));
    aout(self) << " " << chopsticks.back()->id();
  }
  aout(self) << endl;
  // spawn five philosophers
  auto names = std::vector{"Plato"s, "Hume"s, "Kant"s, "Nietzsche"s,
                           "Descartes"s};
  for (size_t i = 0; i < 5; ++i)
    self->spawn(actor_from_state<philosopher_state>, names[i], chopsticks[i],
                chopsticks[(i + 1) % 5]);
}

CAF_MAIN(id_block::dining_philosophers)
