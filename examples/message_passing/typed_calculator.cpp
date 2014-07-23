/******************************************************************************\
 * This example is a very basic, non-interactive math service implemented     *
 * using typed actors.                                                        *
\ ******************************************************************************/

#include <cassert>
#include <iostream>
#include "caf/all.hpp"

using std::endl;
using namespace caf;

namespace {

struct shutdown_request { };
struct plus_request { int a; int b; };
struct minus_request { int a; int b; };

using calculator_type = typed_actor<replies_to<plus_request>::with<int>,
                                    replies_to<minus_request>::with<int>,
                                    replies_to<shutdown_request>::with<void>>;

calculator_type::behavior_type typed_calculator(calculator_type::pointer self) {
  return {
    [](const plus_request& pr) {
      return pr.a + pr.b;
    },
    [](const minus_request& pr) {
      return pr.a - pr.b;
    },
    [=](const shutdown_request&) {
      self->quit();
    }
  };
}

class typed_calculator_class : public calculator_type::base {
 protected:
  behavior_type make_behavior() override {
    return {
      [](const plus_request& pr) {
        return pr.a + pr.b;
      },
      [](const minus_request& pr) {
        return pr.a - pr.b;
      },
      [=](const shutdown_request&) {
        quit();
      }
    };
  }
};

void tester(event_based_actor* self, const calculator_type& testee) {
  self->link_to(testee);
  // will be invoked if we receive an unexpected response message
  self->on_sync_failure([=] {
    aout(self) << "AUT (actor under test) failed" << endl;
    self->quit(exit_reason::user_shutdown);
  });
  // first test: 2 + 1 = 3
  self->sync_send(testee, plus_request{2, 1}).then(
    [=](int r1) {
      assert(r1 == 3);
      // second test: 2 - 1 = 1
      self->sync_send(testee, minus_request{2, 1}).then(
        [=](int r2) {
          assert(r2 == 1);
          // both tests succeeded
          aout(self) << "AUT (actor under test) seems to be ok"
                 << endl;
          self->send(testee, shutdown_request{});
        }
      );
    }
  );
}

} // namespace <anonymous>

int main() {
  // announce custom message types
  announce<shutdown_request>();
  announce<plus_request>(&plus_request::a, &plus_request::b);
  announce<minus_request>(&minus_request::a, &minus_request::b);
  // test function-based impl
  spawn(tester, spawn_typed(typed_calculator));
  await_all_actors_done();
  // test class-based impl
  spawn(tester, spawn_typed<typed_calculator_class>());
  await_all_actors_done();
  // done
  shutdown();
  return 0;
}
