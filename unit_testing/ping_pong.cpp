#include "ping_pong.hpp"

#include "cppa/cppa.hpp"

namespace { int s_pongs = 0; }

using namespace cppa;

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    bool done = false;
    // kickoff
    ping_actor << make_tuple(atom("Pong"), 0); // or: send(ping_actor, 0);
    // invoke rules
    auto pattern =
    (
        on<atom("Ping"), std::int32_t>(9) >> [&]()
        {
            done = true;
        },
        on<atom("Ping"), std::int32_t>() >> [](int v)
        {
            reply(atom("Pong"), v+1);
        }
    );
    // loop
    while (!done) receive(pattern);
    // terminate with non-normal exit reason
    // to force ping actor to quit
    quit(exit_reason::user_defined);
}

void ping()
{
    s_pongs = 0;
    // invoke rule
    auto pattern =
    (
        on<atom("Pong"), std::int32_t>() >> [](std::int32_t v)
        {
            ++s_pongs;
            reply(atom("Ping"), v+1);
        }
    );
    // loop
    for (;;) receive(pattern);
}

int pongs()
{
    return s_pongs;
}
