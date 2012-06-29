#include <vector>
#include <chrono>
#include <sstream>
#include <iostream>

#include "cppa/cppa.hpp"

using std::cout;
using std::endl;
using std::chrono::seconds;

using namespace cppa;

// either taken by a philosopher or available
struct chopstick : sb_actor<chopstick> {

    behavior& init_state; // a reference to available
    behavior available;

    behavior taken_by(const actor_ptr& philos) {
        // create a behavior new on-the-fly
        return (
            on<atom("take"), actor_ptr>() >> [=](actor_ptr other) {
                send(other, atom("busy"), this);
            },
            on(atom("put"), philos) >> [=]() {
                become(&available);
            }
        );
    }

    chopstick() : init_state(available) {
        available = (
            on<atom("take"), actor_ptr>() >> [=](actor_ptr philos) {
                send(philos, atom("taken"), this);
                become(taken_by(philos));
            }
        );
    }

};

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

struct philosopher : sb_actor<philosopher> {

    std::string name; // the name of this philosopher
    actor_ptr left;   // left chopstick
    actor_ptr right;  // right chopstick

    // note: we have to define all behaviors in the constructor because
    //       non-static member initialization are not (yet) implemented in GCC
    behavior thinking;
    behavior hungry;
    behavior denied;
    behavior eating;
    behavior init_state;

    // wait for second chopstick
    behavior waiting_for(const actor_ptr& what) {
        return (
            on(atom("taken"), what) >> [=]() {
                // create message in memory to avoid interleaved
                // messages on the terminal
                std::ostringstream oss;
                oss << name
                    << " has picked up chopsticks with IDs "
                    << left->id()
                    << " and "
                    << right->id()
                    << " and starts to eat\n";
                cout << oss.str();
                // eat some time
                delayed_send(this, seconds(5), atom("think"));
                become(&eating);
            },
            on(atom("busy"), what) >> [=]() {
                send((what == left) ? right : left, atom("put"), this);
                send(this, atom("eat"));
                become(&thinking);
            }
        );
    }

    philosopher(const std::string& n, const actor_ptr& l, const actor_ptr& r)
        : name(n), left(l), right(r) {
        // a philosopher that receives {eat} stops thinking and becomes hungry
        thinking = (
            on(atom("eat")) >> [=]() {
                become(&hungry);
                send(left, atom("take"), this);
                send(right, atom("take"), this);
            }
        );
        // wait for the first answer of a chopstick
        hungry = (
            on(atom("taken"), left) >> [=]() {
                become(waiting_for(right));
            },
            on(atom("taken"), right) >> [=]() {
                become(waiting_for(left));
            },
            on<atom("busy"), actor_ptr>() >> [=]() {
                become(&denied);
            }
        );
        // philosopher was not able to obtain the first chopstick
        denied = (
            on<atom("taken"), actor_ptr>() >> [=](actor_ptr& ptr) {
                send(ptr, atom("put"), this);
                send(this, atom("eat"));
                become(&thinking);
            },
            on<atom("busy"), actor_ptr>() >> [=]() {
                send(this, atom("eat"));
                become(&thinking);
            }
        );
        // philosopher obtained both chopstick and eats (for five seconds)
        eating = (
            on(atom("think")) >> [=]() {
                send(left, atom("put"), this);
                send(right, atom("put"), this);
                delayed_send(this, seconds(5), atom("eat"));
                cout << (  name
                         + " puts down his chopsticks and starts to think\n");
                become(&thinking);
            }
        );
        // philosophers start to think after receiving {think}
        init_state = (
            on(atom("think")) >> [=]() {
                cout << (name + " starts to think\n");
                delayed_send(this, seconds(5), atom("eat"));
                become(&thinking);
            }
        );
    }

};

int main(int, char**) {
    // create five chopsticks
    cout << "chopstick ids:";
    std::vector<actor_ptr> chopsticks;
    for (size_t i = 0; i < 5; ++i) {
        chopsticks.push_back(spawn<chopstick>());
        cout << " " << chopsticks.back()->id();
    }
    cout << endl;
    // a group to address all philosophers
    auto dinner_club = group::anonymous();
    // spawn five philosopher, each joining the Dinner Club
    std::vector<std::string> names = { "Plato", "Hume", "Kant",
                                       "Nietzsche", "Descartes" };
    for (size_t i = 0; i < 5; ++i) {
        spawn_in_group<philosopher>(dinner_club,
                                    names[i],
                                    chopsticks[i],
                                    chopsticks[(i+1) % chopsticks.size()]);
    }
    // tell philosophers to start thinking
    send(dinner_club, atom("think"));
    // real philosophers are never done
    await_all_others_done();
    return 0;
}
