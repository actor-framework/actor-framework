/******************************************************************************
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    C++                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
 *                                                                            *
 * Copyright 2011-2018 Dominik Charousset                                     *
 *                                                                            *
 * Distributed under the terms and conditions of the BSD 3-Clause License or  *
 * (at your option) under the terms and conditions of the Boost Software      *
 * License 1.0. See accompanying files LICENSE and LICENSE_ALTERNATIVE.       *
 *                                                                            *
 * If you did not receive a copy of the license files, see                    *
 * http://opensource.org/licenses/BSD-3-Clause and                            *
 * http://www.boost.org/LICENSE_1_0.txt.                                      *
 ******************************************************************************/

#include <utility>

#include "caf/config.hpp"

#define CAF_SUITE request_response
#include "caf/test/dsl.hpp"

#include "caf/all.hpp"

#define ERROR_HANDLER                                                          \
  [&](error& err) { CAF_FAIL(system.render(err)); }

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
    set_default_handler(reflect);
    return {
      [] {
        // nop
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
  explicit popular_actor(actor_config& cfg, actor  buddy_arg)
      : event_based_actor(cfg),
        buddy_(std::move(buddy_arg)) {
    // don't pollute unit test output with (provoked) warnings
    set_default_handler(drop);
  }

  inline const actor& buddy() const {
    return buddy_;
  }

private:
  actor buddy_;
};

/******************************************************************************\
 *                                test case 1:                                *
 *                                                                            *
 *                  A                  B                  C                   *
 *                  |                  |                  |                   *
 *                  | --(delegate)---> |                  |                   *
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
        return delegate(next, gogo_atom::value);
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
      [=](gogo_atom x) {
        CAF_MESSAGE("forward message to buddy");
        quit();
        return delegate(buddy(), x);
      }
    };
  }
};

class C : public event_based_actor {
public:
  C(actor_config& cfg) : event_based_actor(cfg) {
    // don't pollute unit test output with (provoked) warnings
    set_default_handler(drop);
  }

  behavior make_behavior() override {
    return {
      [=](gogo_atom) -> atom_value {
        CAF_MESSAGE("received `gogo_atom`, about to quit");
        quit();
        return ok_atom::value;
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
      [=](gogo_atom gogo) -> response_promise {
        auto rp = make_response_promise();
        request(buddy(), infinite, gogo).then(
          [=](ok_atom ok) mutable {
            rp.deliver(ok);
            quit();
          }
        );
        return rp;
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
  return {
    [=](idle_atom, actor worker) {
      self->become(
        keep_behavior,
        [=](request_atom task) {
          self->unbecome(); // await next idle message
          return self->delegate(worker, task);
        },
        [](idle_atom) {
          return skip();
        }
      );
    },
    [](request_atom) {
      return skip();
    }
  };
}

struct fixture {
  actor_system_config cfg;
  actor_system system;
  scoped_actor self;
  fixture() : system(cfg), self(system) {
    // nop
  }
};

} // namespace <anonymous>

CAF_TEST_FIXTURE_SCOPE(request_response_tests1, fixture)

CAF_TEST(test_void_res) {
  using testee_a = typed_actor<replies_to<int, int>::with<void>>;
  auto buddy = system.spawn([]() -> testee_a::behavior_type {
    return [](int, int) {
      // nop
    };
  });
  self->request(buddy, infinite, 1, 2).receive(
    [] {
      CAF_MESSAGE("received void res");
    },
    ERROR_HANDLER
  );
}

CAF_TEST(pending_quit) {
  auto mirror = system.spawn([](event_based_actor* ptr) -> behavior {
    ptr->set_default_handler(reflect);
    return {
      [] {
        // nop
      }
    };
  });
  system.spawn([mirror](event_based_actor* ptr) {
    ptr->request(mirror, infinite, 42).then(
      [](int) {
        CAF_ERROR("received result, should've been terminated already");
      },
      [](const error& err) {
        CAF_CHECK_EQUAL(err, sec::request_receiver_down);
      }
    );
    ptr->quit();
  });
}

CAF_TEST(request_float_or_int) {
  int invocations = 0;
  auto foi = self->spawn<float_or_int, linked>();
  self->send(foi, i_atom::value);
  self->receive(
    [](int i) {
      CAF_CHECK_EQUAL(i, 0);
    }
  );
  self->request(foi, infinite, i_atom::value).receive(
    [&](int i) {
      CAF_CHECK_EQUAL(i, 0);
      ++invocations;
    },
    [&](const error& err) {
      CAF_ERROR("Error: " << self->system().render(err));
    }
  );
  self->request(foi, infinite, f_atom::value).receive(
    [&](float f) {
      CAF_CHECK_EQUAL(f, 0.f);
      ++invocations;
    },
    [&](const error& err) {
      CAF_ERROR("Error: " << self->system().render(err));
    }
  );
  CAF_CHECK_EQUAL(invocations, 2);
  CAF_MESSAGE("trigger sync failure");
  self->request(foi, infinite, f_atom::value).receive(
    [&](int) {
      CAF_FAIL("int handler called");
    },
    [&](error& err) {
      CAF_MESSAGE("error received");
      CAF_CHECK_EQUAL(err, sec::unexpected_response);
    }
  );
}

CAF_TEST(request_to_mirror) {
  auto mirror = system.spawn<sync_mirror>();
  self->request(mirror, infinite, 42).receive(
    [&](int value) {
      CAF_CHECK_EQUAL(value, 42);
    },
    ERROR_HANDLER
  );
}

CAF_TEST(request_to_a_fwd2_b_fwd2_c) {
  self->request(self->spawn<A, monitored>(self), infinite,
                go_atom::value, self->spawn<B>(self->spawn<C>())).receive(
    [](ok_atom) {
      CAF_MESSAGE("received 'ok'");
    },
    ERROR_HANDLER
  );
}

CAF_TEST(request_to_a_fwd2_d_fwd2_c) {
  self->request(self->spawn<A, monitored>(self), infinite,
                go_atom::value, self->spawn<D>(self->spawn<C>())).receive(
    [](ok_atom) {
      CAF_MESSAGE("received 'ok'");
    },
    ERROR_HANDLER
  );
}

CAF_TEST(request_to_self) {
  self->request(self, milliseconds(50), no_way_atom::value).receive(
    [&] {
      CAF_ERROR("unexpected empty message");
    },
    [&](const error& err) {
      CAF_MESSAGE("err = " << system.render(err));
      CAF_REQUIRE(err == sec::request_timeout);
    }
  );
}

CAF_TEST(invalid_request) {
  self->request(self->spawn<C>(), milliseconds(500),
                hi_there_atom::value).receive(
    [&](hi_there_atom) {
      CAF_ERROR("C did reply to 'HiThere'");
    },
    [&](const error& err) {
      CAF_REQUIRE_EQUAL(err, sec::unexpected_message);
    }
  );
}

CAF_TEST(client_server_worker_user_case) {
  auto serv = self->spawn<linked>(server);                       // server
  auto work = self->spawn<linked>([]() -> behavior {             // worker
    return {
      [](request_atom) {
        return response_atom::value;
      }
    };
  });
  // first 'idle', then 'request'
  anon_send(serv, idle_atom::value, work);
  self->request(serv, infinite, request_atom::value).receive(
    [&](response_atom) {
      CAF_MESSAGE("received 'response'");
      CAF_CHECK_EQUAL(self->current_sender(), work);
    },
    [&](const error& err) {
      CAF_ERROR("error: " << self->system().render(err));
    }
  );
  // first 'request', then 'idle'
  auto handle = self->request(serv, infinite, request_atom::value);
  send_as(work, serv, idle_atom::value, work);
  handle.receive(
    [&](response_atom) {
      CAF_CHECK_EQUAL(self->current_sender(), work.address());
    },
    [&](const error& err) {
      CAF_ERROR("error: " << self->system().render(err));
    }
  );
}

behavior request_no_then_A(event_based_actor*) {
  return [=](int number) {
    CAF_MESSAGE("got " << number);
  };
}

behavior request_no_then_B(event_based_actor* self) {
  return {
    [=](int number) {
      self->request(self->spawn(request_no_then_A), infinite, number);
    }
  };
}

CAF_TEST(request_no_then) {
  anon_send(system.spawn(request_no_then_B), 8);
}

CAF_TEST(async_request) {
  auto foo = system.spawn([](event_based_actor* ptr) -> behavior {
    auto receiver = ptr->spawn<linked>([](event_based_actor* ptr2) -> behavior {
      return {
        [=](int) {
          return ptr2->make_response_promise();
        }
      };
    });
    ptr->request(receiver, infinite, 1).then(
      [=](int) {}
    );
    return {
      [=](int) {
        CAF_MESSAGE("int received");
        ptr->quit(exit_reason::user_shutdown);
      }
    };
  });
  anon_send(foo, 1);
}

CAF_TEST(skip_responses) {
  auto mirror = system.spawn<sync_mirror>();
  auto future = self->request(mirror, infinite, 42);
  self->send(mirror, 42);
  self->receive([](int x) {
    CAF_CHECK_EQUAL(x, 42);
  });
  // second receive must time out
  self->receive(
    [](int) {
      CAF_FAIL("received response message as ordinary message");
    },
    after(std::chrono::milliseconds(20)) >> [] {
      CAF_MESSAGE("second receive timed out as expected");
    }
  );
  future.receive(
    [](int x) {
      CAF_CHECK_EQUAL(x, 42);
    },
    [&](const error& err) {
      CAF_FAIL(system.render(err));
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()

CAF_TEST_FIXTURE_SCOPE(request_response_tests2, test_coordinator_fixture<>)

CAF_TEST(request_response_in_test_coordinator) {
  auto mirror = sys.spawn<sync_mirror>();
  sched.run();
  sched.inline_next_enqueue();
  // this block would deadlock without inlining the next enqueue
  self->request(mirror, infinite, 23).receive(
    [](int x) {
      CAF_CHECK_EQUAL(x, 23);

    },
    [&](const error& err) {
      CAF_FAIL("unexpected error: " << sys.render(err));
    }
  );
}

CAF_TEST_FIXTURE_SCOPE_END()
