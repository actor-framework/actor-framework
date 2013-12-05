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
    return  (
        on(atom("pong"), arg_match) >> [num_pings](int value) -> any_tuple {
            CPPA_LOGF_ERROR_IF(!self->last_sender(), "last_sender() == nullptr");
            CPPA_LOGF_INFO("received {'pong', " << value << "}");
            //cout << to_string(self->last_dequeued()) << endl;
            if (++s_pongs >= num_pings) {
                CPPA_LOGF_INFO("reached maximum, send {'EXIT', user_defined} "
                               << "to last sender and quit with normal reason");
                send_exit(self->last_sender(), exit_reason::user_shutdown);
                self->quit();
            }
            return {atom("ping"), value};
        },
        others() >> [] {
            CPPA_LOGF_ERROR("unexpected message; "
                            << to_string(self->last_dequeued()));
            self->quit(exit_reason::user_shutdown);
        }
    );
}

behavior pong_behavior() {
    return  (
        on(atom("ping"), arg_match) >> [](int value) -> any_tuple {
            CPPA_LOGF_INFO("received {'ping', " << value << "}");
            return {atom("pong"), value + 1};
        },
        others() >> [] {
            CPPA_LOGF_ERROR("unexpected message; "
                            << to_string(self->last_dequeued()));
            self->quit(exit_reason::user_shutdown);
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

void event_based_ping(size_t num_pings) {
    CPPA_LOGF_TRACE("num_pings = " << num_pings);
    s_pongs = 0;
    become(ping_behavior(num_pings));
}

void pong(actor_ptr ping_actor) {
    CPPA_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
    send(ping_actor, atom("pong"), 0); // kickoff
    receive_loop(pong_behavior());
}

void event_based_pong(actor_ptr ping_actor) {
    CPPA_LOGF_TRACE("ping_actor = " << to_string(ping_actor));
    CPPA_REQUIRE(ping_actor != nullptr);
    send(ping_actor, atom("pong"), 0); // kickoff
    become(pong_behavior());
}
