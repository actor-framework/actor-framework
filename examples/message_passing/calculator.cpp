/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * for both the blocking and the event-based API.                             *
\ ******************************************************************************/

#include <tuple>
#include <cassert>
#include <iostream>

#include "caf/all.hpp"

using std::cout;
using std::endl;
using namespace caf;

// implementation using the blocking API
void blocking_math_fun(blocking_actor* self) {
  bool done = false;
  self->do_receive (
    // "arg_match" matches the parameter types of given lambda expression
    // thus, it's equal to
    // - on<atom("plus"), int, int>()
    // - on(atom("plus"), val<int>, val<int>)
    on(atom("plus"), arg_match) >> [](int a, int b) {
      return std::make_tuple(atom("result"), a + b);
    },
    on(atom("minus"), arg_match) >> [](int a, int b) {
      return std::make_tuple(atom("result"), a - b);
    },
    on(atom("quit")) >> [&]() {
      // note: this actor uses the blocking API, hence self->quit()
      //     would force stack unwinding by throwing an exception
      done = true;
    }
  ).until(done);
}

void calculator(event_based_actor* self) {
  // execute this behavior until actor terminates
  self->become (
    on(atom("plus"), arg_match) >> [](int a, int b) {
      return std::make_tuple(atom("result"), a + b);
    },
    on(atom("minus"), arg_match) >> [](int a, int b) {
      return std::make_tuple(atom("result"), a - b);
    },
    on(atom("quit")) >> [=] {
      // terminate actor with normal exit reason
      self->quit();
    }
  );
}

void tester(event_based_actor* self, const actor& testee) {
  self->link_to(testee);
  // will be invoked if we receive an unexpected response message
  self->on_sync_failure([=] {
    aout(self) << "AUT (actor under test) failed" << endl;
    self->quit(exit_reason::user_shutdown);
  });
  // first test: 2 + 1 = 3
  self->sync_send(testee, atom("plus"), 2, 1).then(
    on(atom("result"), 3) >> [=] {
      // second test: 2 - 1 = 1
      self->sync_send(testee, atom("minus"), 2, 1).then(
        on(atom("result"), 1) >> [=] {
          // both tests succeeded
          aout(self) << "AUT (actor under test) seems to be ok"
                 << endl;
          self->send(testee, atom("quit"));
        }
      );
    }
  );
}

int main() {
  spawn(tester, spawn(calculator));
  await_all_actors_done();
  shutdown();
  return 0;
}
