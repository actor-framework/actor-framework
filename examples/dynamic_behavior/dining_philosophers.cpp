/******************************************************************************\
 * This example is an implementation of the classical Dining Philosophers     *
 * exercise using only libcaf's event-based actor implementation.             *
\******************************************************************************/

#include <map>
#include <thread>
#include <utility>
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
using taken_atom = atom_constant<atom("taken")>;

// atoms for philosopher interface
using eat_atom = atom_constant<atom("eat")>;
using think_atom = atom_constant<atom("think")>;

// a chopstick
using chopstick = typed_actor<replies_to<take_atom>::with<taken_atom, bool>,
                              reacts_to<put_atom>>;

chopstick::behavior_type taken_chopstick(chopstick::pointer,
                                         const strong_actor_ptr&);

// either taken by a philosopher or available
chopstick::behavior_type available_chopstick(chopstick::pointer self) {
  return {
    [=](take_atom) -> std::tuple<taken_atom, bool> {
      self->become(taken_chopstick(self, self->current_sender()));
      return std::make_tuple(taken_atom::value, true);
    },
    [](put_atom) {
      cerr << "chopstick received unexpected 'put'" << endl;
    }
  };
}

chopstick::behavior_type taken_chopstick(chopstick::pointer self,
                                         const strong_actor_ptr& user) {
  return {
    [](take_atom) -> std::tuple<taken_atom, bool> {
      return std::make_tuple(taken_atom::value, false);
    },
    [=](put_atom) {
      if (self->current_sender() == user)
        self->become(available_chopstick(self));
    }
  };
}

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

class philosopher : public event_based_actor {
public:
  philosopher(actor_config& cfg,
              std::string  n,
              chopstick  l,
              chopstick  r)
      : event_based_actor(cfg),
        name_(std::move(n)),
        left_(std::move(l)),
        right_(std::move(r)) {
    // we only accept one message per state and skip others in the meantime
    set_default_handler(skip);
    // a philosopher that receives {eat} stops thinking and becomes hungry
    thinking_.assign(
      [=](eat_atom) {
        become(hungry_);
        send(left_, take_atom::value);
        send(right_, take_atom::value);
      }
    );
    // wait for the first answer of a chopstick
    hungry_.assign(
      [=](taken_atom, bool result) {
        if (result)
          become(granted_);
        else
          become(denied_);
      }
    );
    // philosopher was able to obtain the first chopstick
    granted_.assign(
      [=](taken_atom, bool result) {
        if (result) {
          aout(this) << name_
                     << " has picked up chopsticks with IDs "
                     << left_->id() << " and " << right_->id()
                     << " and starts to eat\n";
          // eat some time
          delayed_send(this, seconds(5), think_atom::value);
          become(eating_);
        } else {
          send(current_sender() == left_ ? right_ : left_, put_atom::value);
          send(this, eat_atom::value);
          become(thinking_);
        }
      }
    );
    // philosopher was *not* able to obtain the first chopstick
    denied_.assign(
      [=](taken_atom, bool result) {
        if (result)
          send(current_sender() == left_ ? left_ : right_, put_atom::value);
        send(this, eat_atom::value);
        become(thinking_);
      }
    );
    // philosopher obtained both chopstick and eats (for five seconds)
    eating_.assign(
      [=](think_atom) {
        send(left_, put_atom::value);
        send(right_, put_atom::value);
        delayed_send(this, seconds(5), eat_atom::value);
        aout(this) << name_
                   << " puts down his chopsticks and starts to think\n";
        become(thinking_);
      }
    );
  }

  const char* name() const override {
    return name_.c_str();
  }

protected:
  behavior make_behavior() override {
    // start thinking
    send(this, think_atom::value);
    // philosophers start to think after receiving {think}
    return (
      [=](think_atom) {
        aout(this) << name_ << " starts to think\n";
        delayed_send(this, seconds(5), eat_atom::value);
        become(thinking_);
      }
    );
  }

private:
  std::string name_;     // the name of this philosopher
  chopstick   left_;     // left chopstick
  chopstick   right_;    // right chopstick
  behavior    thinking_; // initial behavior
  behavior    hungry_;   // tries to take chopsticks
  behavior    granted_;  // has one chopstick and waits for the second one
  behavior    denied_;   // could not get first chopsticks
  behavior    eating_;   // wait for some time, then go thinking again
};

} // namespace <anonymous>

void caf_main(actor_system& system) {
  scoped_actor self{system};
  // create five chopsticks
  aout(self) << "chopstick ids are:";
  std::vector<chopstick> chopsticks;
  for (size_t i = 0; i < 5; ++i) {
    chopsticks.push_back(self->spawn(available_chopstick));
    aout(self) << " " << chopsticks.back()->id();
  }
  aout(self) << endl;
  // spawn five philosophers
  std::vector<std::string> names {"Plato", "Hume", "Kant",
                                  "Nietzsche", "Descartes"};
  for (size_t i = 0; i < 5; ++i)
    self->spawn<philosopher>(names[i], chopsticks[i], chopsticks[(i + 1) % 5]);
}

CAF_MAIN()
