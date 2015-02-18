#include "test.hpp"

#include "caf/none.hpp"

#include "caf/all.hpp"

using namespace std;
using namespace caf;

using std::chrono::milliseconds;

namespace {

using f_atom = atom_constant<atom("f")>;
using i_atom = atom_constant<atom("i")>;
using idle_atom = atom_constant<atom("idle")>;
using request_atom = atom_constant<atom("request")>;
using response_atom = atom_constant<atom("response")>;
using go_atom = atom_constant<atom("go")>;
using gogo_atom = atom_constant<atom("gogo")>;
using gogogo_atom = atom_constant<atom("gogogo")>;
using no_way_atom = atom_constant<atom("NoWay")>;
using hi_there_atom = atom_constant<atom("HiThere")>;

struct sync_mirror : event_based_actor {
  behavior make_behavior() override {
    return {
      others() >> [=] {
        return current_message();
      }
    };
  }
};

// replies to 'f' with 0.0f and to 'i' with 0
struct float_or_int : event_based_actor {
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
  popular_actor(const actor& buddy_arg) : m_buddy(buddy_arg) {
    // nop
  }

  inline const actor& buddy() const {
    return m_buddy;
  }

  void report_failure() {
    send(buddy(), error_atom::value);
    quit();
  }

 private:
  actor m_buddy;
};

/******************************************************************************\
 *                                test case 1:                                *
 *                                                                            *
 *                  A                  B                  C                   *
 *                  |                  |                  |                   *
 *                  | --(sync_send)--> |                  |                   *
 *                  |                  | --(forward)----> |                   *
 *                  |                  X                  |---\               *
 *                  |                                     |   |               *
 *                  |                                     |<--/               *
 *                  | <-------------(reply)-------------- |                   *
 *                  X                                     X                   *
\******************************************************************************/

class A : public popular_actor {
 public:
  A(const actor& buddy_arg) : popular_actor(buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      [=](go_atom, const actor& next) {
        CAF_CHECKPOINT();
        sync_send(next, gogo_atom::value).then(
          [=](atom_value) {
            CAF_CHECKPOINT();
            send(buddy(), ok_atom::value);
            quit();
          }
        );
      },
      others() >> [=] {
        report_failure();
      }
    };
  }
};

class B : public popular_actor {
 public:
  B(const actor& buddy_arg) : popular_actor(buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others() >> [=] {
        CAF_CHECKPOINT();
        forward_to(buddy());
        quit();
      }
    };
  }
};

class C : public event_based_actor {
 public:
  behavior make_behavior() override {
    return {
      [=](gogo_atom) -> atom_value {
        CAF_CHECKPOINT();
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
 *                  | --(sync_send)--> |                  |                   *
 *                  |                  | --(sync_send)--> |                   *
 *                  |                  |                  |---\               *
 *                  |                  |                  |   |               *
 *                  |                  |                  |<--/               *
 *                  |                  | <---(reply)----- |                   *
 *                  | <---(reply)----- |                                      *
 *                  X                  X                                      *
\******************************************************************************/

class D : public popular_actor {
 public:
  D(const actor& buddy_arg) : popular_actor(buddy_arg) {
    // nop
  }

  behavior make_behavior() override {
    return {
      others() >> [=] {
        return sync_send(buddy(), std::move(current_message())).then(
          others() >> [=]() -> message {
            quit();
            return std::move(current_message());
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

class server : public event_based_actor {
 public:
  behavior make_behavior() override {
    auto die = [=] {
      quit(exit_reason::user_shutdown);
    };
    return {
      [=](idle_atom, actor worker) {
        become(
          keep_behavior,
          [=](request_atom) {
            forward_to(worker);
            unbecome(); // await next idle message
          },
          on(idle_atom::value) >> skip_message,
          others() >> die
        );
      },
      on(request_atom::value) >> skip_message,
      others() >> die
    };
  }
};

void test_sync_send() {
  scoped_actor self;
  self->on_sync_failure([&] {
    CAF_FAILURE("received: " << to_string(self->current_message()));
  });
  self->spawn<monitored + blocking_api>([](blocking_actor* s) {
    CAF_LOGC_TRACE("NONE", "main$sync_failure_test", "id = " << s->id());
    int invocations = 0;
    auto foi = s->spawn<float_or_int, linked>();
    s->send(foi, i_atom::value);
    s->receive(
      [](int i) {
        CAF_CHECK_EQUAL(i, 0);
      }
    );
    s->on_sync_failure([=] {
      CAF_FAILURE("received: " << to_string(s->current_message()));
    });
    s->sync_send(foi, i_atom::value).await(
      [&](int i) {
        CAF_CHECK_EQUAL(i, 0);
        ++invocations;
      },
      [&](float) {
        CAF_UNEXPECTED_MSG(s);
      }
    );
    s->sync_send(foi, f_atom::value).await(
      [&](int) {
        CAF_UNEXPECTED_MSG(s);
      },
      [&](float f) {
        CAF_CHECK_EQUAL(f, 0.f);
        ++invocations;
      }
    );
    CAF_CHECK_EQUAL(invocations, 2);
    CAF_PRINT("trigger sync failure");
    // provoke invocation of s->handle_sync_failure()
    bool sync_failure_called = false;
    bool int_handler_called = false;
    s->on_sync_failure([&] { sync_failure_called = true; });
    s->sync_send(foi, f_atom::value).await(
      [&](int) {
        int_handler_called = true;
      }
    );
    CAF_CHECK_EQUAL(sync_failure_called, true);
    CAF_CHECK_EQUAL(int_handler_called, false);
    s->quit(exit_reason::user_shutdown);
  });
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
    },
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  auto mirror = spawn<sync_mirror>();
  bool continuation_called = false;
  self->sync_send(mirror, 42).await([&](int value) {
    continuation_called = true;
    CAF_CHECK_EQUAL(value, 42);
  });
  CAF_CHECK_EQUAL(continuation_called, true);
  self->send_exit(mirror, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  auto non_normal_down_msg = [](down_msg dm) -> optional<down_msg> {
    if (dm.reason != exit_reason::normal) {
      return dm;
    }
    return none;
  };
  auto await_ok_message = [&] {
    self->receive(
      [](ok_atom) {
        CAF_CHECKPOINT();
      },
      [](error_atom) {
        CAF_FAILURE("A didn't receive sync response");
      },
      on(non_normal_down_msg) >> [&](const down_msg& dm) {
        CAF_FAILURE("A exited for reason " << dm.reason);
      }
    );
  };
  self->send(self->spawn<A, monitored>(self),
             go_atom::value, spawn<B>(spawn<C>()));
  await_ok_message();
  CAF_CHECKPOINT();
  self->await_all_other_actors_done();
  self->send(self->spawn<A, monitored>(self),
             go_atom::value, spawn<D>(spawn<C>()));
  await_ok_message();
  CAF_CHECKPOINT();
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  self->timed_sync_send(self, milliseconds(50), no_way_atom::value).await(
    on<sync_timeout_msg>() >> CAF_CHECKPOINT_CB(),
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
  // we should have received two DOWN messages with normal exit reason
  // plus 'NoWay'
  int i = 0;
  self->receive_for(i, 3)(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::normal);
    },
    [](no_way_atom) {
      CAF_CHECKPOINT();
      CAF_PRINT("trigger \"actor did not reply to a "
                "synchronous request message\"");
    },
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(milliseconds(0)) >> CAF_UNEXPECTED_TOUT_CB()
  );
  CAF_CHECKPOINT();
  // mailbox should be empty now
  self->receive(
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self),
    after(milliseconds(0)) >> CAF_CHECKPOINT_CB()
  );
  // check wheter continuations are invoked correctly
  auto c = spawn<C>(); // replies only to 'gogo' messages
  // first test: sync error must occur, continuation must not be called
  bool timeout_occured = false;
  self->on_sync_timeout([&] {
    CAF_CHECKPOINT();
    timeout_occured = true;
  });
  self->on_sync_failure(CAF_UNEXPECTED_MSG_CB_REF(self));
  self->timed_sync_send(c, milliseconds(500), hi_there_atom::value).await(
    on(val<atom_value>) >> [&] {
      cout << "C did reply to 'HiThere'" << endl;
    }
  );
  CAF_CHECK_EQUAL(timeout_occured, true);
  self->on_sync_failure(CAF_UNEXPECTED_MSG_CB_REF(self));
  self->sync_send(c, gogo_atom::value).await(
    [](gogogo_atom) {
      CAF_CHECKPOINT();
    }
  );
  self->send_exit(c, exit_reason::user_shutdown);
  self->await_all_other_actors_done();
  CAF_CHECKPOINT();
  // test use case 3
  self->spawn<monitored + blocking_api>([](blocking_actor* s) { // client
    auto serv = s->spawn<server, linked>();                     // server
    auto work = s->spawn<linked>([]() -> behavior {             // worker
      return {
        [](request_atom) {
          return response_atom::value;
        }
      };
    });
    // first 'idle', then 'request'
    anon_send(serv, idle_atom::value, work);
    s->sync_send(serv, request_atom::value).await(
      [=](response_atom) {
        CAF_CHECKPOINT();
        CAF_CHECK_EQUAL(s->current_sender(), work);
      },
      others() >> [&] {
        CAF_PRINTERR("unexpected message: " << to_string(s->current_message()));
      }
    );
    // first 'request', then 'idle'
    auto handle = s->sync_send(serv, request_atom::value);
    send_as(work, serv, idle_atom::value);
    handle.await(
      [=](response_atom) {
        CAF_CHECKPOINT();
        CAF_CHECK_EQUAL(s->current_sender(), work);
      },
      others() >> CAF_UNEXPECTED_MSG_CB(s)
    );
    s->send(s, "Ever danced with the devil in the pale moonlight?");
    // response: {'EXIT', exit_reason::user_shutdown}
    s->receive_loop(
      others() >> CAF_UNEXPECTED_MSG_CB(s)
    );
  });
  self->receive(
    [&](const down_msg& dm) {
      CAF_CHECK_EQUAL(dm.reason, exit_reason::user_shutdown);
    },
    others() >> CAF_UNEXPECTED_MSG_CB_REF(self)
  );
}

} // namespace <anonymous>

int main() {
  CAF_TEST(test_sync_send);
  test_sync_send();
  await_all_actors_done();
  CAF_CHECKPOINT();
  return CAF_TEST_RESULT();
}
