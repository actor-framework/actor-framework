#include <iostream>

#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/to_string.hpp"

namespace { size_t s_pongs = 0; }

using std::cout;
using std::endl;
using namespace cppa;

size_t pongs() {
    return s_pongs;
}

void ping(size_t num_pings) {
    s_pongs = 0;
    do_receive (
        on<atom("pong"), int>() >> [&](int value) {
            //cout << to_string(self->last_dequeued()) << endl;
            if (++s_pongs == num_pings) {
                reply(atom("EXIT"), exit_reason::user_defined);
            }
            else {
                reply(atom("ping"), value);
            }
        },
        others() >> []() {
            cout << "unexpected message; "
                 << __FILE__ << " line " << __LINE__ << ": "
                 << to_string(self->last_dequeued()) << endl;
            self->quit(exit_reason::user_defined);
        }
    )
    .until(gref(s_pongs) == num_pings);
}

actor_ptr spawn_event_based_ping(size_t num_pings) {
    s_pongs = 0;
    struct impl : public sb_actor<impl> {
        behavior init_state;
        impl(size_t num_pings) {
            init_state = (
                on<atom("pong"), int>() >> [num_pings, this](int value) {
                    //cout << to_string(self->last_dequeued()) << endl;
                    if (++s_pongs >= num_pings) {
                        reply(atom("EXIT"), exit_reason::user_defined);
                        quit();
                    }
                    else {
                        reply(atom("ping"), value);
                    }
                },
                others() >> []() {
                    cout << "unexpected message; "
                         << __FILE__ << " line " << __LINE__ << ": "
                         << to_string(self->last_dequeued()) << endl;
                    self->quit(exit_reason::user_defined);
                }
            );
        }
    };
    return spawn<impl>(num_pings);
}

void pong(actor_ptr ping_actor) {
    // kickoff
    send(ping_actor, atom("pong"), 0);
    receive_loop (
        on<atom("ping"), int>() >> [](int value) {
            //cout << to_string(self->last_dequeued()) << endl;
            reply(atom("pong"), value + 1);
        },
        others() >> []() {
            cout << "unexpected message; "
                 << __FILE__ << " line " << __LINE__ << ": "
                 << to_string(self->last_dequeued()) << endl;
            self->quit(exit_reason::user_defined);
        }
    );
}

actor_ptr spawn_event_based_pong(actor_ptr ping_actor) {
    CPPA_REQUIRE(ping_actor.get() != nullptr);
    struct impl : public sb_actor<impl> {
        behavior init_state;
        impl() {
            init_state = (
                on<atom("ping"), int>() >> [](int value) {
                    //cout << to_string(self->last_dequeued()) << endl;
                    reply(atom("pong"), value + 1);
                },
                others() >> []() {
                    cout << "unexpected message; "
                         << __FILE__ << " line " << __LINE__ << ": "
                         << to_string(self->last_dequeued()) << endl;
                    self->quit(exit_reason::user_defined);
                }
            );
        }
    };
    auto pptr = spawn<impl>();
    // kickoff
    ping_actor->enqueue(pptr.get(), make_any_tuple(atom("pong"), 0));
    return pptr;
}
