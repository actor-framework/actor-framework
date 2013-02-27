#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace std;
using namespace cppa;
using namespace cppa::placeholders;

struct sync_mirror : sb_actor<sync_mirror> {
    behavior init_state = (
        others() >> [] { reply_tuple(self->last_dequeued()); }
    );
};

// replies to 'f' with 0.0f and to 'i' with 0
struct float_or_int : sb_actor<float_or_int> {
    behavior init_state = (
        on(atom("f")) >> [] { reply(0.0f); },
        on(atom("i")) >> [] { reply(0); }
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
                sync_send(next, atom("gogo")).then([=] {
                    send(buddy(), atom("success"));
                    quit();
                });
            },
            others() >> [=] { report_failure(); }
        );
    }
};

#ifdef __clang__
struct B : sb_actor<B,popular_actor> {
    B(const actor_ptr& buddy) : sb_actor<B,popular_actor>(buddy) { }
    behavior init_state = (
        others() >> [=] {
            forward_to(buddy());
            quit();
        }
    );
};
#else
struct B : popular_actor {
    B(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            others() >> [=] {
                forward_to(buddy());
                quit();
            }
        );
    }
};
#endif

struct C : sb_actor<C> {
    behavior init_state = (
        on(atom("gogo")) >> [=] {
            reply(atom("gogogo"));
            self->quit();
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
    response_handle m_handle;
    D(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            others() >> [=] {
                m_handle = make_response_handle();
                sync_send_tuple(buddy(), last_dequeued()).then([=] {
                    m_handle.apply(last_dequeued());
                    quit();
                });
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
        auto die = [=] { quit(exit_reason::user_defined); };
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



int main() {
    CPPA_TEST(test__sync_send);
    self->on_sync_failure([] {
        CPPA_ERROR("received: " << to_string(self->last_dequeued()));
    });
    spawn<monitored + blocking_api>([&] {
        int invocations = 0;
        auto foi = spawn<float_or_int, linked>();
        send(foi, atom("i"));
        receive(on_arg_match >> [](int i) { CPPA_CHECK_EQUAL(i, 0); });
        self->on_sync_failure([] {
            CPPA_ERROR("received: " << to_string(self->last_dequeued()));
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
        // provoke invocation of self->handle_sync_failure()
        bool sync_failure_called = false;
        bool int_handler_called = false;
        self->on_sync_failure([&] {
            sync_failure_called = true;
        });
        sync_send(foi, atom("f")).await(
            on<int>() >> [&] { int_handler_called = true; }
        );
        CPPA_CHECK_EQUAL(sync_failure_called, true);
        CPPA_CHECK_EQUAL(int_handler_called, false);
        self->quit(exit_reason::user_defined);
    });
    receive (
        on(atom("DOWN"), exit_reason::user_defined) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    auto mirror = spawn<sync_mirror>();
    bool continuation_called = false;
    sync_send(mirror, 42)
    .then([](int value) { CPPA_CHECK_EQUAL(value, 42); })
    .continue_with([&] { continuation_called = true; });
    self->exec_behavior_stack();
    CPPA_CHECK_EQUAL(continuation_called, true);
    quit_actor(mirror, exit_reason::user_defined);
    await_all_others_done();
    CPPA_CHECKPOINT();
    auto await_success_message = [&] {
        receive (
            on(atom("success")) >> CPPA_CHECKPOINT_CB(),
            on(atom("failure")) >> CPPA_ERROR_CB("A didn't receive sync response"),
            on(atom("DOWN"), arg_match).when(_x2 != exit_reason::normal)
            >> [&](uint32_t err) {
                CPPA_ERROR("A exited for reason " << err);
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
        on(atom("NoWay")) >> CPPA_CHECKPOINT_CB(),
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
    .then(CPPA_ERROR_CB("C replied to 'HiThere'!"))
    .continue_with(CPPA_ERROR_CB("bad continuation"));
    self->exec_behavior_stack();
    CPPA_CHECK_EQUAL(timeout_occured, true);
    self->on_sync_failure(CPPA_UNEXPECTED_MSG_CB());
    sync_send(c, atom("gogo")).then(CPPA_CHECKPOINT_CB())
                              .continue_with(CPPA_CHECKPOINT_CB());
    self->exec_behavior_stack();
    quit_actor(c, exit_reason::user_defined);
    await_all_others_done();
    CPPA_CHECKPOINT();

    // test use case 3
    spawn<monitored>([] {                    // client
        auto s = spawn<server, linked>();    // server
        auto w = spawn<linked>([] {  // worker
            become(on(atom("request")) >> []{ reply(atom("response")); });
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
        // response: {'EXIT', exit_reason::user_defined}
        receive_loop(others() >> CPPA_UNEXPECTED_MSG_CB());
    });
    receive (
        on(atom("DOWN"), exit_reason::user_defined) >> CPPA_CHECKPOINT_CB(),
        others() >> CPPA_UNEXPECTED_MSG_CB()
    );
    await_all_others_done();
    CPPA_CHECKPOINT();
    shutdown();
    return CPPA_TEST_RESULT();
}
