// This example is an implementation of the classical Dining Philosophers
// exercise using only CAF's event-based actor implementation.

#include "caf/actor_from_state.hpp"
#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/mail_cache.hpp"

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

// Configures how many messages a philosopher can stash away.
constexpr size_t mail_cache_size = 20;

// A chopstick: either taken by a philosopher or available.
struct chopstick_trait {
  using signatures
    = type_list<result<taken_atom, bool>(take_atom), result<void>(put_atom)>;
};
using chopstick_actor = typed_actor<chopstick_trait>;

struct chopstick_state {
  explicit chopstick_state(chopstick_actor::pointer selfptr) : self(selfptr) {
    // nop
  }

  chopstick_actor::behavior_type make_behavior() {
    return {
      [this](take_atom) -> result<taken_atom, bool> {
        self->become(keep_behavior, taken(self->current_sender()));
        return {taken_atom_v, true};
      },
      [](put_atom) { cerr << "chopstick received unexpected 'put'" << endl; },
    };
  }

  chopstick_actor::behavior_type taken(const strong_actor_ptr& user) {
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

  chopstick_actor::pointer self;
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
  philosopher_state(event_based_actor* selfptr, std::string n,
                    chopstick_actor l, chopstick_actor r)
    : self(selfptr),
      name(std::move(n)),
      cache(selfptr, mail_cache_size),
      left(std::move(l)),
      right(std::move(r)) {
    // Default handler for pushing messages to the cache.
    auto skip_unmatched = [this](message msg) { cache.stash(std::move(msg)); };
    // A philosopher that receives {eat} stops thinking and becomes hungry.
    thinking = behavior{
      [this](eat_atom) {
        self->become(hungry);
        cache.unstash();
        self->mail(take_atom_v).send(left);
        self->mail(take_atom_v).send(right);
      },
      skip_unmatched,
    };
    // Wait for the first answer of a chopstick.
    hungry = behavior{
      [this](taken_atom, bool result) {
        if (result)
          self->become(granted);
        else
          self->become(denied);
        cache.unstash();
      },
      skip_unmatched,
    };
    // Philosopher was able to obtain the first chopstick.
    granted = behavior{
      [this](taken_atom, bool result) {
        if (result) {
          self->println("{} has picked up chopsticks with "
                        "IDs {} and {} and starts to eat",
                        name, left->id(), right->id());
          // Eat some time.
          self->mail(think_atom_v).delay(5s).send(self);
          self->become(eating);
        } else {
          self->mail(put_atom_v)
            .send(self->current_sender() == left ? right : left);
          self->mail(eat_atom_v).send(self);
          self->become(thinking);
        }
        cache.unstash();
      },
      skip_unmatched,
    };
    // Philosopher was *not* able to obtain the first chopstick.
    denied = behavior{
      [this](taken_atom, bool result) {
        if (result)
          self->mail(put_atom_v)
            .send(self->current_sender() == left ? left : right);
        self->mail(eat_atom_v).send(self);
        self->become(thinking);
        cache.unstash();
      },
      skip_unmatched,
    };
    // Philosopher obtained both chopstick and eats (for five seconds).
    eating = behavior{
      [this](think_atom) {
        self->mail(put_atom_v).send(left);
        self->mail(put_atom_v).send(right);
        self->mail(eat_atom_v).delay(5s).send(self);
        self->println("{} puts down his chopsticks and starts to think", name);
        self->become(thinking);
        cache.unstash();
      },
      skip_unmatched,
    };
  }

  behavior make_behavior() {
    self->println("{} starts to think", name);
    self->mail(eat_atom_v).delay(5s).send(self);
    return thinking;
  }

  event_based_actor* self; // handle to the actor itself
  std::string name;        // the name of this philosopher
  mail_cache cache;        // stashes messages to handle them later
  chopstick_actor left;    // left chopstick
  chopstick_actor right;   // right chopstick
  behavior thinking;       // initial behavior
  behavior hungry;         // tries to take chopsticks
  behavior granted;        // has one chopstick and waits for the second one
  behavior denied;         // could not get first chopsticks
  behavior eating;         // wait for some time, then go thinking again
};

void caf_main(actor_system& sys) {
  // Create five chopsticks.
  sys.println("chopstick ids are:");
  auto chopsticks = std::vector<chopstick_actor>{};
  for (size_t i = 0; i < 5; ++i) {
    chopsticks.push_back(sys.spawn(actor_from_state<chopstick_state>));
    sys.println("- {}", chopsticks.back()->id());
  }
  // Create five philosophers.
  auto names = std::vector{"Plato"s, "Hume"s, "Kant"s, "Nietzsche"s,
                           "Descartes"s};
  for (size_t i = 0; i < 5; ++i)
    sys.spawn(actor_from_state<philosopher_state>, names[i], chopsticks[i],
              chopsticks[(i + 1) % 5]);
}

CAF_MAIN(id_block::dining_philosophers)
