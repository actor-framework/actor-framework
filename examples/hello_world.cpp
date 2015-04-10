#include <string>
#include <iostream>

#include "caf/all.hpp"

using std::endl;
using std::string;

using namespace caf;

behavior mirror(event_based_actor* self) {
  // return the (initial) actor behavior
  return {
    // a handler for messages containing a single string
    // that replies with a string
    [=](const string& what) -> string {
      // prints "Hello World!" via aout (thread-safe cout wrapper)
      aout(self) << what << endl;
      // terminates this actor ('become' otherwise loops forever)
      self->quit();
      // reply "!dlroW olleH"
      return string(what.rbegin(), what.rend());
    }
  };
}

void hello_world(event_based_actor* self, const actor& buddy) {
  // send "Hello World!" to our buddy ...
  self->sync_send(buddy, "Hello World!").then(
    // ... wait for a response ...
    [=](const string& what) {
      // ... and print it
      aout(self) << what << endl;
    }
  );
}

int main() {
  // create a new actor that calls 'mirror()'
  auto mirror_actor = spawn(mirror);
  // create another actor that calls 'hello_world(mirror_actor)';
  spawn(hello_world, mirror_actor);
  // wait until all other actors we have spawned are done
  await_all_actors_done();
  // run cleanup code before exiting main
  shutdown();
}
