#include "test.hpp"
#include "cppa/cppa.hpp"

using namespace cppa;

/******************************************************************************\
 *                                 test case:                                 *
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

struct A : event_based_actor {
    actor_ptr m_parent;
    A(const actor_ptr& parent) : m_parent(parent) { }
    void init() {
        auto report_failure = [=] {
            send(m_parent, atom("failure"));
            quit();
        };
        become (
            on(atom("go"), arg_match) >> [=](const actor_ptr& next) {
                handle_response(sync_send(next, atom("gogo"))) (
                    on(atom("gogogo")) >> [=] {
                        send(m_parent, atom("success"));
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

struct B : event_based_actor {
    actor_ptr m_buddy;
    B(const actor_ptr& buddy) : m_buddy(buddy) { }
    void init() {
        become (
            others() >> [=] {
                forward_to(m_buddy);
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
    return CPPA_TEST_RESULT;
}
