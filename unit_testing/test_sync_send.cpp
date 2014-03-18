#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::placeholders;

struct sync_mirror : sb_actor<sync_mirror> {
    behavior init_state = (
        others() >> [] { return self->last_dequeued(); }
    );
};

// replies to 'f' with 0.0f and to 'i' with 0
struct float_or_int : sb_actor<float_or_int> {
    behavior init_state = (
        on(atom("f")) >> [] { return 0.0f; },
        on(atom("i")) >> [] { return 0; }
    );
};

struct popular_actor : event_based_actor { // popular actors have a buddy
    actor_ptr m_buddy;
    popular_actor(const actor_ptr& buddy) : m_buddy(buddy) { }
    inline const actor_ptr& buddy() const { return m_buddy; }
    void report_failure() {
        send(buddy(), atom("failure"));
        self->quit();
    }
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

struct A : popular_actor {
    A(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            on(atom("go"), arg_match) >> [=](const actor_ptr& next) {
                CPPA_CHECKPOINT();
                sync_send(next, atom("gogo")).then([=] {
                    CPPA_CHECKPOINT();
                    send(buddy(), atom("success"));
                    quit();
                });
            },
            others() >> [=] { report_failure(); }
        );
    }
};

struct B : popular_actor {
    B(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            others() >> [=] {
                CPPA_CHECKPOINT();
                forward_to(buddy());
                quit();
            }
        );
    }
};

struct C : sb_actor<C> {
    behavior init_state = (
        on(atom("gogo")) >> [=]() -> atom_value {
            self->quit();
            return atom("gogogo");
        }
    );
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

struct D : popular_actor {
    D(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            others() >> [=] {
                /*
                response_handle handle = make_response_handle();
                sync_send_tuple(buddy(), last_dequeued()).then([=] {
                    reply_to(handle, last_dequeued());
                    quit();
                });
                //*/
                return sync_send_tuple(buddy(), last_dequeued()).then([=]() -> any_tuple {
                    quit();
                    return last_dequeued();
                });//*/
            }
        );
    }
};


/******************************************************************************\
 *                                test case 3:                                *
 *                                                                            *
 *                Client             Server             Worker                *
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

struct server : event_based_actor {

    void init() {
        auto die = [=] { quit(exit_reason::user_shutdown); };
        become (
            on(atom("idle")) >> [=] {
                auto worker = last_sender();
                become (
                    keep_behavior,
                    on(atom("request")) >> [=] {
                        forward_to(worker);
                        unbecome(); // await next idle message
                    },
                    on(atom("idle")) >> skip_message,
                    others() >> die
                );
            },
            on(atom("request")) >> skip_message,
            others() >> die
        );
    }

};

void compile_time_optional_variant_check() {
    typedef optional_variant<std::tuple<int, float>,
                             std::tuple<float, int, int>>
            msg_type;
    sync_send(self, atom("msg")).then([](msg_type) {});
}

int main() {
    CPPA_TEST(test_sync_send);
    self->on_sync_failure([] {
        CPPA_FAILURE("received: " << to_string(self->last_dequeued()));
    });
    spawn<monitored + blocking_api>([] {
        CPPA_LOGC_TRACE("NONE", "main$sync_failure_test", "id = " << self->id());
        int invocations = 0;
        auto foi = spawn<float_or_int, linked>();
        send(foi, atom("i"));
        receive(on_arg_match >> [](int i) { CPPA_CHECK_EQUAL(i, 0); });
        self->on_sync_failure([] {
            CPPA_FAILURE("received: " << to_string(self->last_dequeued()));
        });
        sync_send(foi, atom("i")).then(
            [&](int i) { CPPA_CHECK_EQUAL(i, 0); ++invocations; },
            [&](float) { CPPA_UNEXPECTED_MSG(); }
        )
        .continue_with([&] {
            sync_send(foi, atom("f")).then(
                [&](int) { CPPA_UNEXPECTED_MSG(); },
                [&](float f) { CPPA_CHECK_EQUAL(f, 0); ++invocations; }
            );
        });
        self->exec_behavior_stack();
        CPPA_CHECK_EQUAL(invocations, 2);
        CPPA_PRINT("trigger sync failure");
        // provoke invocation of self->handle_sync_failure()
        bool sync_failure_called = false;
        bool int_handler_called = false;
        self->on_sync_failure([&] {
            sync_failure_called = true;
        });
        sync_send(foi, atom("f")).await(
            on<int>() >> [&] {
                int_handler_called = true;
            }
        );
        CPPA_CHECK_EQUAL(sync_failure_called, true);
        CPPA_CHECK_EQUAL(int_handler_called, false);
        self->quit(exit_reason::user_shutdown);
    });
    receive (
        on(atom("DOWN"), exit_reason::user_shutdown) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    auto mirror = spawn<sync_mirror>();
    bool continuation_called = false;
    sync_send(mirror, 42)
    .then([](int value) { CPPA_CHECK_EQUAL(value, 42); })
    .continue_with([&] { continuation_called = true; });
    self->exec_behavior_stack();
    CPPA_CHECK_EQUAL(continuation_called, true);
    send_exit(mirror, exit_reason::user_shutdown);
    await_all_others_done();
    CPPA_CHECKPOINT();
    auto await_success_message = [&] {
        receive (
            on(atom("success")) >> CPPA_CHECKPOINT_CB(),
            on(atom("failure")) >> CPPA_FAILURE_CB("A didn't receive sync response"),
            on(atom("DOWN"), arg_match).when(_x2 != exit_reason::normal)
            >> [&](uint32_t err) {
                CPPA_FAILURE("A exited for reason " << err);
            }
        );
    };
    send(spawn<A, monitored>(self), atom("go"), spawn<B>(spawn<C>()));
    await_success_message();
    CPPA_CHECKPOINT();
    await_all_others_done();
    send(spawn<A, monitored>(self), atom("go"), spawn<D>(spawn<C>()));
    await_success_message();
    CPPA_CHECKPOINT();
    await_all_others_done();
    CPPA_CHECKPOINT();
    timed_sync_send(self, std::chrono::milliseconds(50), atom("NoWay")).await(
        on(atom("TIMEOUT")) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    // we should have received two DOWN messages with normal exit reason
    // plus 'NoWay'
    int i = 0;
    receive_for(i, 3) (
        on(atom("DOWN"), exit_reason::normal) >> CPPA_CHECKPOINT_CB(),
        on(atom("NoWay")) >> [] {
            CPPA_CHECKPOINT();
            CPPA_PRINT("trigger \"actor did not reply to a "
                       "synchronous request message\"");
        },
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(std::chrono::seconds(0)) >> CPPA_UNEXPECTED_TOUT_CB()
    );
    CPPA_CHECKPOINT();
    // mailbox should be empty now
    receive (
        others() >> CPPA_UNEXPECTED_MSG_CB(),
        after(std::chrono::seconds(0)) >> CPPA_CHECKPOINT_CB()
    );
    // check wheter continuations are invoked correctly
    auto c = spawn<C>(); // replies only to 'gogo' messages
    // first test: sync error must occur, continuation must not be called
    bool timeout_occured = false;
    self->on_sync_timeout([&] { timeout_occured = true; });
    self->on_sync_failure(CPPA_UNEXPECTED_MSG_CB());
    timed_sync_send(c, std::chrono::milliseconds(500), atom("HiThere"))
    .then(CPPA_FAILURE_CB("C replied to 'HiThere'!"))
    .continue_with(CPPA_FAILURE_CB("continuation erroneously invoked"));
    self->exec_behavior_stack();
    CPPA_CHECK_EQUAL(timeout_occured, true);
    self->on_sync_failure(CPPA_UNEXPECTED_MSG_CB());
    sync_send(c, atom("gogo")).then(CPPA_CHECKPOINT_CB())
                              .continue_with(CPPA_CHECKPOINT_CB());
    self->exec_behavior_stack();
    send_exit(c, exit_reason::user_shutdown);
    await_all_others_done();
    CPPA_CHECKPOINT();

    // test use case 3
    spawn<monitored + blocking_api>([] {  // client
        auto s = spawn<server, linked>(); // server
        auto w = spawn<linked>([] {       // worker
            become(on(atom("request")) >> []{ return atom("response"); });
        });
        // first 'idle', then 'request'
        send_as(w, s, atom("idle"));
        sync_send(s, atom("request")).await(
            on(atom("response")) >> [=] {
                CPPA_CHECKPOINT();
                CPPA_CHECK_EQUAL(self->last_sender(), w);
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        // first 'request', then 'idle'
        auto handle = sync_send(s, atom("request"));
        send_as(w, s, atom("idle"));
        receive_response(handle) (
            on(atom("response")) >> [=] {
                CPPA_CHECKPOINT();
                CPPA_CHECK_EQUAL(self->last_sender(), w);
            },
            others() >> CPPA_UNEXPECTED_MSG_CB()
        );
        send(s, "Ever danced with the devil in the pale moonlight?");
        // response: {'EXIT', exit_reason::user_shutdown}
        receive_loop(others() >> CPPA_UNEXPECTED_MSG_CB());
    });
    receive (
        on(atom("DOWN"), exit_reason::user_shutdown) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    await_all_others_done();
    CPPA_CHECKPOINT();
    shutdown();
    return CPPA_TEST_RESULT();
}
