// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE messaging

#include "caf/event_based_actor.hpp"

#include "core-test.hpp"

using namespace caf;
using namespace std::literals;

using self_ptr = event_based_actor*;

namespace {

struct fixture : test_coordinator_fixture<> {
  actor uut1;
  actor uut2;
  disposable dis;
  bool had_message = false;
};

} // namespace

BEGIN_FIXTURE_SCOPE(fixture)

SCENARIO("send transfers a message from one actor to another") {
  GIVEN("two actors: uut1 and uut2") {
    WHEN("sending a message from uu2 to uu1") {
      THEN("uut1 calls the appropriate message handler") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int i) {
              had_message = true;
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { self->send(uut1, 42); });
        run();
        CHECK(had_message);
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
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          self->delayed_send(uut1, 1s, 42);
        });
        sched.run();
        CHECK(!had_message);
        advance_time(1s);
        sched.run();
        CHECK(had_message);
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
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          self->scheduled_send(uut1, timeout, 42);
        });
        sched.run();
        CHECK(!had_message);
        advance_time(1s);
        sched.run();
        CHECK(had_message);
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
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), nullptr);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { self->anon_send(uut1, 42); });
        run();
        CHECK(had_message);
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
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), nullptr);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          self->delayed_anon_send(uut1, 1s, 42);
        });
        sched.run();
        CHECK(!had_message);
        advance_time(1s);
        sched.run();
        CHECK(had_message);
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
              CHECK_EQ(i, 42);
              CHECK_EQ(self->current_sender(), nullptr);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          self->scheduled_anon_send(uut1, timeout, 42);
        });
        sched.run();
        CHECK(!had_message);
        advance_time(1s);
        sched.run();
        CHECK(had_message);
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
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          dis = self->delayed_send(uut1, 1s, 42);
        });
        sched.run();
        CHECK(!had_message);
        dis.dispose();
        advance_time(1s);
        run();
        CHECK(!had_message);
      }
    }
    WHEN("when disposing the message of delayed_anon_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          dis = self->delayed_anon_send(uut1, 1s, 42);
        });
        sched.run();
        CHECK(!had_message);
        dis.dispose();
        advance_time(1s);
        run();
        CHECK(!had_message);
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
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          dis = self->scheduled_send(uut1, timeout, 42);
        });
        sched.run();
        CHECK(!had_message);
        dis.dispose();
        advance_time(1s);
        run();
        CHECK(!had_message);
      }
    }
    WHEN(
      "when disposing the message of scheduled_anon_send before it arrives") {
      THEN("uut1 receives no message") {
        uut1 = sys.spawn([this](self_ptr self) -> behavior {
          return {
            [this, self](int) {
              had_message = true;
              CHECK_EQ(self->current_sender(), uut2);
            },
            [](float) { CAF_FAIL("float handler called"); },
          };
        });
        uut2 = sys.spawn([this](self_ptr self) { //
          auto timeout = self->clock().now() + 1s;
          dis = self->scheduled_anon_send(uut1, timeout, 42);
        });
        sched.run();
        CHECK(!had_message);
        dis.dispose();
        advance_time(1s);
        run();
        CHECK(!had_message);
      }
    }
  }
}

END_FIXTURE_SCOPE()
