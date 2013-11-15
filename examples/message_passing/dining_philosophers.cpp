/******************************************************************************\
 * This example is an implementation of the classical Dining Philosophers     *
 * exercise using only libcppa's event-based actor implementation.            *
\******************************************************************************/

#include <map>
#include <vector>
#include <chrono>
#include <sstream>
#include <iostream>

#include "cppa/cppa.hpp"

using std::chrono::seconds;

using namespace std;
using namespace cppa;

// either taken by a philosopher or available
void chopstick() {
    become(
        on(atom("take"), arg_match) >> [=](const actor_ptr& philos) {
            // tell philosopher it took this chopstick
            send(philos, atom("taken"), self);
            // await 'put' message and reject other 'take' messages
            become(
                // allows us to return to the previous behavior
                keep_behavior,
                on(atom("take"), arg_match) >> [=](const actor_ptr& other) {
                    send(other, atom("busy"), self);
                },
                on(atom("put"), philos) >> [=] {
                    // return to previous behaivor, i.e., await next 'take'
                    unbecome();
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

struct philosopher : event_based_actor {

    std::string name; // the name of this philosopher
    actor_ptr left;   // left chopstick
    actor_ptr right;  // right chopstick

    // note: we have to define all behaviors in the constructor because
    //       non-static member initialization are not (yet) implemented in GCC
    behavior thinking;
    behavior hungry;
    behavior denied;
    behavior eating;

    // wait for second chopstick
    behavior waiting_for(const actor_ptr& what) {
        return (
            on(atom("taken"), what) >> [=] {
                aout << name
                     << " has picked up chopsticks with IDs "
                     << left->id()
                     << " and "
                     << right->id()
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
        );
    }

    philosopher(const std::string& n, const actor_ptr& l, const actor_ptr& r)
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
            on<atom("busy"), actor_ptr>() >> [=] {
                become(denied);
            }
        );
        // philosopher was not able to obtain the first chopstick
        denied = (
            on(atom("taken"), arg_match) >> [=](const actor_ptr& ptr) {
                send(ptr, atom("put"), this);
                send(this, atom("eat"));
                become(thinking);
            },
            on<atom("busy"), actor_ptr>() >> [=] {
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
                aout << name
                     << " puts down his chopsticks and starts to think\n";
                become(thinking);
            }
        );
    }

    void init() {
        // philosophers start to think after receiving {think}
        become (
            on(atom("think")) >> [=] {
                aout << name << " starts to think\n";
                delayed_send(this, seconds(5), atom("eat"));
                become(thinking);
            }
        );
        // start thinking
        send(this, atom("think"));
    }

};

int main(int, char**) {
    // create five chopsticks
    aout << "chopstick ids are:";
    std::vector<actor_ptr> chopsticks;
    for (size_t i = 0; i < 5; ++i) {
        chopsticks.push_back(spawn(chopstick));
        aout << " " << chopsticks.back()->id();
    }
    aout << endl;
    // spawn five philosophers
    std::vector<std::string> names { "Plato", "Hume", "Kant",
                                     "Nietzsche", "Descartes" };
    for (size_t i = 0; i < 5; ++i) {
        spawn<philosopher>(names[i], chopsticks[i], chopsticks[(i+1)%5]);
    }
    // real philosophers are never done
    await_all_actors_done();
    shutdown();
    return 0;
}
