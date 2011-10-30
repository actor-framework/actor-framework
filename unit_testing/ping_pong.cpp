#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"

namespace { int s_pongs = 0; }

using namespace cppa;

void pong(actor_ptr ping_actor)
{
    link(ping_actor);
    // kickoff
    send(ping_actor, atom("Pong"), static_cast<std::int32_t>(0));
    // invoke rules
    receive_loop
    (
        on(atom("Ping"), val<actor_ptr>(), std::int32_t(9)) >> []()
        //on<atom("Ping"), std::int32_t>(9) >> []()
        {
            // terminate with non-normal exit reason
            // to force ping actor to quit
            quit(exit_reason::user_defined);
        },
        on<atom("Ping"), actor_ptr, std::int32_t>() >> [](actor_ptr teammate,
                                                          std::int32_t value)
        //on<atom("Ping"), std::int32_t>() >> [](int value)
        {
            send(teammate, atom("Ping"), self(), value + 1);
        }
    );
}

void ping()
{
    s_pongs = 0;
    // invoke rule
    receive_loop
    (
        on<atom("Pong"), actor_ptr, std::int32_t>() >> [](actor_ptr teammate,
                                                          std::int32_t value)
        {
            ++s_pongs;
            send(teammate, atom("Ping"), self(), value + 1);
        }
    );
}

int pongs()
{
    return s_pongs;
}
