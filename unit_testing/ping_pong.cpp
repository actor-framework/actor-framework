#include <iostream>

#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/logging.hpp"
#include "cppa/sb_actor.hpp"
#include "cppa/to_string.hpp"

using std::cout;
using std::endl;
using namespace cppa;

namespace {

size_t s_pongs = 0;

behavior ping_behavior(size_t num_pings) {
    CPPA_LOGF_TRACE("");
    return  (
        on(atom("pong"), arg_match) >> [num_pings](int value) {
            CPPA_LOGF_ERROR_IF(!self->last_sender(), "last_sender() == nullptr");
            //cout << to_string(self->last_dequeued()) << endl;
            if (++s_pongs >= num_pings) {
                quit_actor(self->last_sender(), exit_reason::user_defined);
                self->quit();
            }
            else reply(atom("ping"), value);
        },
        others() >> [] {
            CPPA_LOGF_ERROR("unexpected message; "
                            << to_string(self->last_dequeued()));
            cout << "unexpected message; "
                 << __FILE__ << " line " << __LINE__ << ": "
                 << to_string(self->last_dequeued()) << endl;
            self->quit(exit_reason::user_defined);
        }
    );
}

behavior pong_behavior() {
    CPPA_LOGF_TRACE("");
    return  (
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

} // namespace <anonymous>

size_t pongs() {
    return s_pongs;
}

void ping(size_t num_pings) {
    CPPA_LOGF_TRACE("num_pings = " << num_pings);
    s_pongs = 0;
    receive_loop(ping_behavior(num_pings));
}

actor_ptr spawn_event_based_ping(size_t num_pings) {
    CPPA_LOGF_TRACE("num_pings = " << num_pings);
    s_pongs = 0;
    return spawn([=] {
        become(ping_behavior(num_pings));
    });
}

void pong(actor_ptr ping_actor) {
    CPPA_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
    // kickoff
    send(ping_actor, atom("pong"), 0);
    receive_loop (pong_behavior());
}

actor_ptr spawn_event_based_pong(actor_ptr ping_actor) {
    CPPA_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
    CPPA_REQUIRE(ping_actor.get() != nullptr);
    return  factory::event_based([=] {
                self->become(pong_behavior());
                send(ping_actor, atom("pong"), 0);
            }).spawn();
}
