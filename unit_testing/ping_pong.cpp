#include <iostream>

#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"

namespace { int s_pongs = 0; }

using std::cout;
using std::endl;
using namespace cppa;

int pongs()
{
    return s_pongs;
}

void ping()
{
    s_pongs = 0;
    receive_loop
    (
        on<atom("pong"), int>() >> [](int value)
        {
            ++s_pongs;
            reply(atom("ping"), value + 1);
        }
    );
}

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    // kickoff
    send(ping_actor, atom("pong"), 0);
    receive_loop
    (
        on(atom("ping"), 9) >> []()
        {
            // terminate with non-normal exit reason
            // to force ping actor to quit
            quit(exit_reason::user_defined);
        },
        on<atom("ping"), int>() >> [](int value)
        {
            reply(atom("pong"), value + 1);
        }
    );
}
