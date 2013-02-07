#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace cppa;
using namespace cppa::placeholders;

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

int main() {
    CPPA_TEST(test__sync_send);
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
    send(spawn_monitor<A>(self), atom("go"), spawn<B>(spawn<C>()));
    await_success_message();
    await_all_others_done();
    send(spawn_monitor<A>(self), atom("go"), spawn<D>(spawn<C>()));
    await_success_message();
    await_all_others_done();
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
    shutdown();
    return CPPA_TEST_RESULT();
}
