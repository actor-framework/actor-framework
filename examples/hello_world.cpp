#include <string>
#include <iostream>
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;

behavior mirror(untyped_actor* self) {
    // wait for messages
    return (
        // invoke this lambda expression if we receive a string
        on_arg_match >> [=](const string& what) -> string {
            // prints "Hello World!" via aout (thread-safe cout wrapper)
            aout << what << endl;
            // terminates this actor ('become' otherwise loops forever)
            self->quit();
            // reply "!dlroW olleH"
            return string(what.rbegin(), what.rend());
        }
    );
}

void hello_world(untyped_actor* self, const actor& buddy) {
    // send "Hello World!" to our buddy ...
    self->sync_send(buddy, "Hello World!").then(
        // ... and wait for a response
        on_arg_match >> [=](const string& what) {
            // prints "!dlroW olleH"
            aout << what << endl;
        }
    );
}

int main() {
    // create a new actor that calls 'mirror()'
    auto mirror_actor = spawn(mirror);
    // create another actor that calls 'hello_world(mirror_actor)'
    spawn(hello_world, mirror_actor);
    // wait until all other actors we have spawned are done
    await_all_actors_done();
    // run cleanup code before exiting main
    shutdown();
}
