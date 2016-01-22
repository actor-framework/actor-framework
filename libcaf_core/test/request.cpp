/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright (C) 2011 - 2015                                                  *
 * Dominik Charousset <dominik.charousset (at) haw-hamburg.de>                *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include "caf/config.hpp"

#define CAF_SUITE request
#include "caf/test/unit_test.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using std::chrono::milliseconds;

namespace {

using f_atom = atom_constant<atom("f")>;
using i_atom = atom_constant<atom("i")>;
using idle_atom = atom_constant<atom("idle")>;
using error_atom = atom_constant<atom("error")>;
using request_atom = atom_constant<atom("request")>;
using response_atom = atom_constant<atom("response")>;
using go_atom = atom_constant<atom("go")>;
using gogo_atom = atom_constant<atom("gogo")>;
using gogogo_atom = atom_constant<atom("gogogo")>;
using no_way_atom = atom_constant<atom("NoWay")>;
using hi_there_atom = atom_constant<atom("HiThere")>;

struct sync_mirror : event_based_actor {
  sync_mirror(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others >> [=] {
        return current_message();
      }
    };
  }
};

// replies to 'f' with 0.0f and to 'i' with 0
struct float_or_int : event_based_actor {
  float_or_int(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [](f_atom) {
        return 0.0f;
      },
      [](i_atom) {
        return 0;
      }
    };
  }
};

class popular_actor : public event_based_actor { // popular actors have a buddy
public:
  explicit popular_actor(actor_config& cfg, const actor& buddy_arg)
      : event_based_actor(cfg),
        buddy_(buddy_arg) {
    // nop
  }

  inline const actor& buddy() const {
    return buddy_;
  }

  void report_failure() {
    send(buddy(), error_atom::value);
    quit();
  }

private:
  actor buddy_;
};

/******************************************************************************\
 *                                test case 1:                                *
 *                                                                            *
 *                  A                  B                  C                   *
 *                  |                  |                  |                   *
 *                  | ---(request)---> |                  |                   *
 *                  |                  | --(forward)----> |                   *
 *                  |                  X                  |---\               *
 *                  |                                     |   |               *
 *                  |                                     |<--/               *
 *                  | <-------------(reply)-------------- |                   *
 *                  X                                     X                   *
\******************************************************************************/

class A : public popular_actor {
public:
  explicit A(actor_config& cfg, const actor& buddy_arg)
      : popular_actor(cfg, buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](go_atom, const actor& next) {
        request(next, gogo_atom::value).then(
          [=](atom_value) {
            CAF_MESSAGE("send `ok_atom` to buddy");
            send(buddy(), ok_atom::value);
            quit();
          }
        );
      },
      others >> [=] {
        report_failure();
      }
    };
  }
};

class B : public popular_actor {
public:
  explicit B(actor_config& cfg, const actor& buddy_arg)
      : popular_actor(cfg, buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others >> [=] {
        CAF_MESSAGE("forward message to buddy");
        forward_to(buddy());
        quit();
      }
    };
  }
};

class C : public event_based_actor {
public:
  C(actor_config& cfg) : event_based_actor(cfg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](gogo_atom) -> atom_value {
        CAF_MESSAGE("received `gogo_atom`, about to quit");
        quit();
        return gogogo_atom::value;
      }
    };
  }
};

/******************************************************************************\
 *                                test case 2:                                *
 *                                                                            *
 *                  A                  D                  C                   *
 *                  |                  |                  |                   *
 *                  | ---(request)---> |                  |                   *
 *                  |                  | ---(request)---> |                   *
 *                  |                  |                  |---\               *
 *                  |                  |                  |   |               *
 *                  |                  |                  |<--/               *
 *                  |                  | <---(reply)----- |                   *
 *                  | <---(reply)----- |                                      *
 *                  X                  X                                      *
\******************************************************************************/

class D : public popular_actor {
public:
  explicit D(actor_config& cfg, const actor& buddy_arg)
      : popular_actor(cfg, buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others >> [=] {
        return request(buddy(), std::move(current_message())).then(
          [=](gogogo_atom x) -> gogogo_atom {
            quit();
            return x;
          }
        );
      }
    };
  }
};

/******************************************************************************\
 *                                test case 3:                                *
 *                                                                            *
 *                Client            Server              Worker                *
 *                  |                  |                  |                   *
 *                  |                  | <---(idle)------ |                   *
 *                  | ---(request)---> |                  |                   *
 *                  |                  | ---(request)---> |                   *
 *                  |                  |                  |---\               *
 *                  |                  X                  |   |               *
 *                  |                                     |<--/               *
 *                  | <------------(response)------------ |                   *
 *                  X                                                         *
\******************************************************************************/

behavior server(event_based_actor* self) {
  auto die = [=] {
    self->quit(exit_reason::user_shutdown);
  };
  return {
    [=](idle_atom, actor worker) {
      self->become(
        keep_behavior,
        [=](request_atom) {
          self->forward_to(worker);
          self->unbecome(); // await next idle message
        },
        [](idle_atom) {
          return skip_message();
        },
        others >> [=] {
          CAF_ERROR("Unexpected message:" << self->current_message());
          die();
        }
      );
    },
    [](request_atom) {
      return skip_message();
    },
    others >> [=] {
      CAF_ERROR("Unexpected message:" << self->current_message());
      die();
    }
  };
}

struct fixture {
  actor_system system;
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(atom_tests, fixture)

CAF_TEST(test_void_res) {
  using testee_a = typed_actor<replies_to<int, int>::with<void>>;
  auto buddy = system.spawn([]() -> testee_a::behavior_type {
    return [](int, int) {
      // nop
    };
  });
  scoped_actor self{system};
  self->request(buddy, 1, 2).receive(
    [] {
      CAF_MESSAGE("received void res");
    }
  );
  self->send_exit(buddy, exit_reason::kill);
}

CAF_TEST(pending_quit) {
  auto mirror = system.spawn([](event_based_actor* self) -> behavior {
    return {
      others >> [=] {
        self->quit();
        return std::move(self->current_message());
      }
    };
  });
  system.spawn([mirror](event_based_actor* self) {
    self->request(mirror, 42).then(
      [](int) {
        CAF_ERROR("received result, should've been terminated already");
      },
      [](const error& err) {
        CAF_CHECK(err == sec::request_receiver_down);
      }
    );
    self->quit();
  });
}

CAF_TEST(request) {
  scoped_actor self{system};
  self->spawn<monitored + blocking_api>([](blocking_actor* s) {
    int invocations = 0;
    auto foi = s->spawn<float_or_int, linked>();
    s->send(foi, i_atom::value);
    s->receive(
      [](int i) {
        CAF_CHECK_EQUAL(i, 0);
      }
    );
    s->request(foi, i_atom::value).receive(
      [&](int i) {
        CAF_CHECK_EQUAL(i, 0);
        ++invocations;
      },
      [&](const error& err) {
        CAF_ERROR("Error: " << s->system().render(err));
      }
    );
    s->request(foi, f_atom::value).receive(
      [&](float f) {
        CAF_CHECK_EQUAL(f, 0.f);
        ++invocations;
      },
      [&](const error& err) {
        CAF_ERROR("Error: " << s->system().render(err));
      }
    );
    CAF_CHECK_EQUAL(invocations, 2);
    CAF_MESSAGE("trigger sync failure");
    // provoke invocation of s->handle_sync_failure()
    bool error_handler_called = false;
    bool int_handler_called = false;
    s->request(foi, f_atom::value).receive(
      [&](int) {
        int_handler_called = true;
      },
      [&](const error&) {
        CAF_MESSAGE("error received");
        error_handler_called = true;
      }
    );
    CAF_CHECK_EQUAL(error_handler_called, true);
    CAF_CHECK_EQUAL(int_handler_called, false);
    s->quit(exit_reason::user_shutdown);
  });
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
    },
    others >> [&] {
      CAF_ERROR("Unexpected message: " << self->current_message());
    }
  );
  auto mirror = system.spawn<sync_mirror>();
  bool continuation_called = false;
  self->request(mirror, 42).receive([&](int value) {
    continuation_called = true;
    CAF_CHECK_EQUAL(value, 42);
  });
  CAF_CHECK_EQUAL(continuation_called, true);
  self->send_exit(mirror, exit_reason::user_shutdown);
  CAF_MESSAGE("block on `await_all_other_actors_done");
  self->await_all_other_actors_done();
  CAF_MESSAGE("`await_all_other_actors_done` finished");
  auto await_ok_message = [&] {
    self->receive(
      [](ok_atom) {
        CAF_MESSAGE("received `ok_atom`");
      },
      [](error_atom) {
        CAF_ERROR("A didn't receive sync response");
      },
      [&](const down_msg& dm) -> maybe<skip_message_t> {
        if (dm.reason == exit_reason::normal) {
          return skip_message();
        }
        CAF_ERROR("A exited for reason " << dm.reason);
        return none;
      }
    );
  };
  self->send(self->spawn<A, monitored>(self),
             go_atom::value, self->spawn<B>(self->spawn<C>()));
  CAF_MESSAGE("block on `await_ok_message`");
  await_ok_message();
  CAF_MESSAGE("`await_ok_message` finished");
  self->await_all_other_actors_done();
  self->send(self->spawn<A, monitored>(self),
             go_atom::value, self->spawn<D>(self->spawn<C>()));
  CAF_MESSAGE("block on `await_ok_message`");
  await_ok_message();
  CAF_MESSAGE("`await_ok_message` finished");
  CAF_MESSAGE("block on `await_all_other_actors_done`");
  self->await_all_other_actors_done();
  CAF_MESSAGE("`await_all_other_actors_done` finished");
  self->request(self, no_way_atom::value).receive(
    [&](int) {
      CAF_ERROR("Unexpected message");
    },
    after(milliseconds(50)) >> [] {
      CAF_MESSAGE("Got timeout");
    }
  );
  // we should have received two DOWN messages with normal exit reason
  // plus 'NoWay'
  int i = 0;
  self->receive_for(i, 3)(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    },
    [](no_way_atom) {
      CAF_MESSAGE("trigger \"actor did not reply to a "
                "synchronous request message\"");
    },
    others >> [&] {
      CAF_ERROR("Unexpected message: " << self->current_message());
    },
    after(milliseconds(0)) >> [] {
      CAF_ERROR("Unexpected timeout");
    }
  );
  // mailbox should be empty now
  self->receive(
    others >> [&] {
      CAF_ERROR("Unexpected message: " << self->current_message());
    },
    after(milliseconds(0)) >> [] {
      CAF_MESSAGE("Mailbox is empty, all good");
    }
  );
  // check wheter continuations are invoked correctly
  auto c = self->spawn<C>(); // replies only to 'gogo' messages
  // first test: sync error must occur, continuation must not be called
  bool timeout_occured = false;
  self->request(c, milliseconds(500), hi_there_atom::value).receive(
    [&](hi_there_atom) {
      CAF_ERROR("C did reply to 'HiThere'");
    },
    [&](const error& err) {
      CAF_LOG_TRACE("");
      CAF_ERROR("Error: " << self->system().render(err));
    },
    after(milliseconds(500)) >> [&] {
      CAF_MESSAGE("timeout occured");
      timeout_occured = true;
    }
  );
  CAF_CHECK_EQUAL(timeout_occured, true);
  self->request(c, gogo_atom::value).receive(
    [](gogogo_atom) {
      CAF_MESSAGE("received `gogogo_atom`");
    },
    [&](const error& err) {
      CAF_LOG_TRACE("");
      CAF_ERROR("Error: " << self->system().render(err));
    }
  );
  self->send_exit(c, exit_reason::user_shutdown);
  CAF_MESSAGE("block on `await_all_other_actors_done`");
  self->await_all_other_actors_done();
  CAF_MESSAGE("`await_all_other_actors_done` finished");
  // test use case 3
  self->spawn<monitored + blocking_api>([](blocking_actor* s) { // client
    auto serv = s->spawn<linked>(server);                       // server
    auto work = s->spawn<linked>([]() -> behavior {             // worker
      return {
        [](request_atom) {
          return response_atom::value;
        }
      };
    });
    // first 'idle', then 'request'
    anon_send(serv, idle_atom::value, work);
    s->request(serv, request_atom::value).receive(
      [&](response_atom) {
        CAF_MESSAGE("received `response_atom`");
        CAF_CHECK(s->current_sender() == work);
      },
      [&](const error& err) {
        CAF_ERROR("Error: " << s->system().render(err));
      }
    );
    // first 'request', then 'idle'
    auto handle = s->request(serv, request_atom::value);
    send_as(work, serv, idle_atom::value, work);
    handle.receive(
      [&](response_atom) {
        CAF_CHECK(s->current_sender() == work);
      },
      [&](const error& err) {
        CAF_ERROR("Error: " << s->system().render(err));
      }
    );
    s->quit(exit_reason::user_shutdown);
  });
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
    },
    others >> [&] {
      CAF_ERROR("Unexpected message: " << self->current_message());
    }
  );
}

behavior snyc_send_no_then_A(event_based_actor * self) {
  return [=](int number) {
    CAF_MESSAGE("got " << number);
    self->quit();
  };
}

behavior snyc_send_no_then_B(event_based_actor * self) {
  return {
    [=](int number) {
      self->request(self->spawn(snyc_send_no_then_A), number);
      self->quit();
    }
  };
}

CAF_TEST(request_no_then) {
  anon_send(system.spawn(snyc_send_no_then_B), 8);
}

CAF_TEST(async_request) {
  auto foo = system.spawn([](event_based_actor* self) -> behavior {
    auto receiver = self->spawn<linked>([](event_based_actor* self) -> behavior{
      return {
        [=](int) {
          return self->make_response_promise();
        }
      };
    });
    self->request(receiver, 1).then(
      [=](int) {}
    );
    return {
      [=](int) {
        CAF_MESSAGE("int received");
        self->quit(exit_reason::user_shutdown);
      }
    };
  });
  anon_send(foo, 1);
}

CAF_TEST_FIXTURE_SCOPE_END()
