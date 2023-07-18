#include "caf/actor_ostream.hpp"
#include "caf/actor_system.hpp"
#include "caf/caf_main.hpp"
#include "caf/event_based_actor.hpp"

#include <iostream>
#include <string>

using namespace caf;

behavior mirror(event_based_actor* self) {
  // return the (initial) actor behavior
  return {
    // a handler for messages containing a single string
    // that replies with a string
    [=](const std::string& what) -> std::string {
      // prints "Hello World!" via aout (thread-safe cout wrapper)
      aout(self) << what << std::endl;
      // reply "!dlroW olleH"
      return std::string{what.rbegin(), what.rend()};
    },
  };
}

void hello_world(event_based_actor* self, const actor& buddy) {
  // send "Hello World!" to our buddy ...
  self->request(buddy, std::chrono::seconds(10), "Hello World!")
    .then(
      // ... wait up to 10s for a response ...
      [=](const std::string& what) {
        // ... and print it
        aout(self) << what << std::endl;
      });
}

void caf_main(actor_system& sys) {
  // create a new actor that calls 'mirror()'
  auto mirror_actor = sys.spawn(mirror);
  // create another actor that calls 'hello_world(mirror_actor)';
  sys.spawn(hello_world, mirror_actor);
  // the system will wait until both actors are done before exiting the program
}

// creates a main function for us that calls our caf_main
CAF_MAIN()
