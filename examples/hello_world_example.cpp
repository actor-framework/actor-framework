#include <string>
#include <iostream>
#include "cppa/cppa.hpp"

using namespace cppa;

void echo_actor() {
    // wait for a message
    receive (
        // invoke this lambda expression if we receive a string
        on<std::string>() >> [](const std::string& what) {
            // prints "Hello World!"
            std::cout << what << std::endl;
            // replies "!dlroW olleH"
            reply(std::string(what.rbegin(), what.rend()));
        }
    );
}

int main() {
    // create a new actor that invokes the function echo_actor
    auto hello_actor = spawn(echo_actor);
    // send "Hello World!" to our new actor
    // note: libcppa converts string literals to std::string
    send(hello_actor, "Hello World!");
    // wait for a response and print it
    receive (
        on<std::string>() >> [](const std::string& what) {
            // prints "!dlroW olleH"
            std::cout << what << std::endl;
        }
    );
    // wait until all other actors we've spawned are done
    await_all_others_done();
    // done
    return 0;
}
