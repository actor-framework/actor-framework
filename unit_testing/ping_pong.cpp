#include <iostream>

#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"

namespace { int s_pongs = 0; }

using std::cout;
using std::endl;
using namespace cppa;

void pong(actor_ptr ping_actor);

void ping()
{
    s_pongs = 0;
    actor_ptr pong_actor;
    auto response = make_tuple(atom("Ping"), 0);
    // invoke rule
    receive_loop
    (
        on<atom("Pong"), std::int32_t>() >> [&](std::int32_t value)
        {
            ++s_pongs;
            get_ref<1>(response) = value + 1;
            last_sender() << response;
        }
    );
}

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    auto pong_tuple = make_tuple(atom("Pong"), 0);
    // kickoff
    ping_actor << pong_tuple;
    // invoke rules
    receive_loop
    (
        on(atom("Ping"), 9) >> []()
        {
            // terminate with non-normal exit reason
            // to force ping actor to quit
            quit(exit_reason::user_defined);
        },
        on<atom("Ping"), int>() >> [&](int value)
        {
            get_ref<1>(pong_tuple) = value + 1;
            last_sender() << pong_tuple;
        }
    );
}

int pongs()
{
    return s_pongs;
}
