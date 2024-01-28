// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/all.hpp"

using namespace caf;
using namespace std::literals;

namespace {

SCENARIO("an actor system shuts down after the last actor terminates") {
  GIVEN("an actor system and a detached actor") {
    WHEN("the actor sets no behavior") {
      auto ran = std::make_shared<bool>(false);
      THEN("the actor terminates immediately and the system shuts down") {
        actor_system_config cfg;
        actor_system sys{cfg};
        sys.spawn<detached>([=] { *ran = true; });
      }
      check(*ran);
    }
    WHEN("the actor uses delayed_send but ignores the message") {
      auto ran = std::make_shared<bool>(false);
      THEN("the actor terminates immediately and the system shuts down") {
        actor_system_config cfg;
        actor_system sys{cfg};
        sys.spawn<detached>([=](event_based_actor* self) {
          *ran = true;
          self->delayed_send(self, 1h, ok_atom_v);
        });
      }
      check(*ran);
    }
    WHEN("the actor uses delayed_send and waits for the message") {
      auto ran = std::make_shared<bool>(false);
      auto message_handled = std::make_shared<bool>(false);
      THEN("the system waits for the actor to handle its message") {
        actor_system_config cfg;
        actor_system sys{cfg};
        sys.spawn<detached>([=](event_based_actor* self) -> behavior {
          *ran = true;
          self->delayed_send(self, 1ns, ok_atom_v);
          return {
            [=](ok_atom) {
              *message_handled = true;
              self->quit();
            },
          };
        });
      }
      check(*ran);
      check(*message_handled);
    }
    WHEN("the actor uses run_delayed() to wait some time") {
      auto ran = std::make_shared<bool>(false);
      auto timeout_handled = std::make_shared<bool>(false);
      THEN("the system waits for the actor to handle the timeout") {
        actor_system_config cfg;
        actor_system sys{cfg};
        sys.spawn<detached>([=](event_based_actor* self) -> behavior {
          *ran = true;
          self->run_delayed(1ns, [=] {
            *timeout_handled = true;
            self->quit();
          });
          return {
            [](int) {
              // Dummy handler to force the actor to stay alive until we call
              // self->quit in the run_delayed lambda.
            },
          };
        });
      }
      check(*ran);
      check(*timeout_handled);
    }
    WHEN("the actor uses after() to wait some time") {
      auto ran = std::make_shared<bool>(false);
      auto timeout_handled = std::make_shared<bool>(false);
      THEN("the system waits for the actor to handle the timeout") {
        actor_system_config cfg;
        actor_system sys{cfg};
        sys.spawn<detached>([=](event_based_actor* self) -> behavior {
          *ran = true;
          return {
            after(1ns) >>
              [=] {
                *timeout_handled = true;
                self->quit();
              },
          };
        });
      }
      check(*ran);
      check(*timeout_handled);
    }
  }
}

} // namespace
