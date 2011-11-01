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
    // invoke rule
    receive_loop
    (
        on<atom("Pong"), std::int32_t>() >> [&](std::int32_t value)
        {
            ++s_pongs;
            auto msg = tuple_cast<atom_value, std::int32_t>(std::move(last_received()));
            get_ref<0>(msg) = atom("Ping");
            get_ref<1>(msg) = value + 1;
            pong_actor->enqueue(std::move(msg));
            //send(pong_actor, atom("Ping"), value + 1);
        },
        on<atom("Hello"), actor_ptr>() >> [&](actor_ptr sender)
        {
            pong_actor = sender;
        }
    );
}

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    // tell ping who we are
    send(ping_actor, atom("Hello"), self());
    // kickoff
    send(ping_actor, atom("Pong"), static_cast<std::int32_t>(0));
    // invoke rules
    receive_loop
    (
        on(atom("Ping"), std::int32_t(9)) >> []()
        {
            // terminate with non-normal exit reason
            // to force ping actor to quit
            quit(exit_reason::user_defined);
        },
        on<atom("Ping"), std::int32_t>() >> [&](std::int32_t value)
        {
            auto msg = tuple_cast<atom_value, std::int32_t>(std::move(last_received()));
            get_ref<0>(msg) = atom("Pong");
            get_ref<1>(msg) = value + 1;
            ping_actor->enqueue(std::move(msg));
            //send(ping_actor, atom("Pong"), value + 1);
        }
    );
}

int pongs()
{
    return s_pongs;
}
