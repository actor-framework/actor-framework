// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/main/LICENSE.

#include "caf/test/fixture/deterministic.hpp"
#include "caf/test/scenario.hpp"
#include "caf/test/test.hpp"

#include "caf/anon_mail.hpp"
#include "caf/event_based_actor.hpp"

using namespace caf;
using namespace std::literals;

using self_ptr = event_based_actor*;

namespace {

struct fixture : test::fixture::deterministic {
  actor uut1;
  actor uut2;
  disposable dis;
  bool had_message = false;
};

} // namespace

WITH_FIXTURE(fixture) {

SCENARIO("send transfers a message from one actor to another") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { self->mail(42).send(uut1); });
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("delayed_send transfers the message after a relative timeout") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          self->mail(42).delay(1s).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        advance_time(1s);
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("scheduled_send transfers the message after an absolute timeout") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          self->mail(42).schedule(timeout).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        advance_time(1s);
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("anon_send hides the sender of a message") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), nullptr);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this] { anon_mail(42).send(uut1); });
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("delayed_anon_send hides the sender of a message") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), nullptr);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this] { anon_mail(42).delay(1s).send(uut1); });
        dispatch_messages();
        check(!had_message);
        trigger_timeout();
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("scheduled_anon_send hides the sender of a message") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              check_eq(i, 42);
              check_eq(self->current_sender(), nullptr);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          anon_mail(42).schedule(timeout).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        advance_time(1s);
        dispatch_messages();
        check(had_message);
      }
    }
  }
}

SCENARIO("a delayed message may be canceled before its timeout") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("when disposing the message of delayed_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          dis = self->mail(42).delay(1s).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        dis.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!had_message);
      }
    }
    WHEN("when disposing the message of delayed_anon_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this] { dis = anon_mail(42).delay(1s).send(uut1); });
        dispatch_messages();
        check(!had_message);
        dis.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!had_message);
      }
    }
  }
}

SCENARIO("a scheduled message may be canceled before its timeout") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("when disposing the message of scheduled_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          dis = self->mail(42).schedule(timeout).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        dis.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!had_message);
      }
    }
    WHEN(
      "when disposing the message of scheduled_anon_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              check_eq(self->current_sender(), uut2);
            },
            [this](float) { fail("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          dis = anon_mail(42).schedule(timeout).send(uut1);
        });
        dispatch_messages();
        check(!had_message);
        dis.dispose();
        advance_time(1s);
        dispatch_messages();
        check(!had_message);
      }
    }
  }
}

} // WITH_FIXTURE(fixture)
