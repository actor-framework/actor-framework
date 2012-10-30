#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace cppa;

struct popular_actor : event_based_actor { // popular actors have a buddy
    actor_ptr m_buddy;
    popular_actor(const actor_ptr& buddy) : m_buddy(buddy) { }
    inline const actor_ptr& buddy() const { return m_buddy; }
};

void report_failure() {
    local_actor* s = self;
    send(static_cast<popular_actor*>(s)->buddy(), atom("failure"));
    self->quit();
}

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
                sync_send(next, atom("gogo")).then (
                    on(atom("gogogo")) >> [=] {
                        send(buddy(), atom("success"));
                        quit();
                    },
                    others() >> report_failure,
                    after(std::chrono::seconds(1)) >> report_failure
                );
            },
            others() >> report_failure
        );
    }
};

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

struct C : event_based_actor {
    void init() {
        become (
            on(atom("gogo")) >> [=] {
                reply(atom("gogogo"));
                quit();
            }
        );
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

struct D : popular_actor {
    response_handle m_handle;
    D(const actor_ptr& buddy) : popular_actor(buddy) { }
    void init() {
        become (
            others() >> [=] {
                m_handle = make_response_handle();
                sync_send_tuple(buddy(), last_dequeued()).then(
                    others() >> [=] {
                        m_handle.apply(last_dequeued());
                        quit();
                    },
                    after(std::chrono::seconds(1)) >> report_failure
                );
            }
        );
    }
};

int main() {
    CPPA_TEST(test__sync_send);
    send(spawn<A>(self), atom("go"), spawn<B>(spawn<C>()));
    receive (
        on(atom("success")) >> [&] { },
        on(atom("failure")) >> [&] {
            CPPA_ERROR("A didn't receive a sync response");
        }
    );
    await_all_others_done();
    send(spawn<A>(self), atom("go"), spawn<D>(spawn<C>()));
    receive (
        on(atom("success")) >> [&] { },
        on(atom("failure")) >> [&] {
            CPPA_ERROR("A didn't receive a sync response");
        }
    );
    await_all_others_done();
    shutdown();
    return CPPA_TEST_RESULT;
}
