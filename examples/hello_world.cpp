#include <string>
#include <iostream>
#include "cppa/cppa.hpp"

using namespace cppa;

void mirror() {
    // wait for messages
    become (
        // invoke this lambda expression if we receive a string
        on_arg_match >> [](const std::string& what) {
            // prints "Hello World!"
            std::cout << what << std::endl;
            // replies "!dlroW olleH"
            reply(std::string(what.rbegin(), what.rend()));
            // terminates this actor
            self->quit();
        }
    );
}

void hello_world(const actor_ptr& buddy) {
    // send "Hello World!" to the mirror
    send(buddy, "Hello World!");
    // wait for messages
    become (
        on_arg_match >> [](const std::string& what) {
            // prints "!dlroW olleH"
            std::cout << what << std::endl;
            // terminate this actor
            self->quit();
        }
    );
}

int main() {
    // create a new actor that calls 'mirror()'
    auto mirror_actor = spawn(mirror);
    // create another actor that calls 'hello_world(mirror_actor)'
    spawn(hello_world, mirror_actor);
    // wait until all other actors we have spawned are done
    await_all_others_done();
    // run cleanup code before exiting main
    shutdown();
    return 0;
}
