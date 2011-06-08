#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"

namespace { int s_pongs = 0; }

using namespace cppa;

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    // kickoff
    ping_actor << make_tuple(atom("Pong"), 0); // or: send(ping_actor, 0);
    // invoke rules
    auto pattern =
    (
        on<atom("Ping"), std::int32_t>(9) >> []()
        {
            // terminate with non-normal exit reason
            // to force ping actor to quit
            quit(exit_reason::user_defined);
        },
        on<atom("Ping"), std::int32_t>() >> [](int v)
        {
            reply(atom("Pong"), v+1);
        }
    );
    // loop
    for (;;) receive(pattern);
}

void ping()
{
    s_pongs = 0;
    // invoke rule
    auto pattern =
    (
        on<atom("Pong"), std::int32_t>() >> [](std::int32_t value)
        {
            ++s_pongs;
            reply(atom("Ping"), value + 1);
        },
        others() >> []()
        {
            throw std::runtime_error(  "unexpected message: "
                                     + to_string(last_received()));
        }

    );
    // loop
    for (;;) receive(pattern);
}

int pongs()
{
    return s_pongs;
}
