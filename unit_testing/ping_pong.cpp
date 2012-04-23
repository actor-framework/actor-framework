#include <iostream>

#include "ping_pong.hpp"

#include "cppa/cppa.hpp"
#include "cppa/to_string.hpp"

namespace { size_t s_pongs = 0; }

using std::cout;
using std::endl;
using namespace cppa;

size_t pongs()
{
    return s_pongs;
}

void ping(size_t num_pings)
{
    s_pongs = 0;
    do_receive
    (
        on<atom("pong"), int>() >> [&](int value)
        {
//cout << to_string(self->last_dequeued()) << endl;
            if (++s_pongs == num_pings)
            {
                reply(atom("EXIT"), exit_reason::user_defined);
            }
            else
            {
                reply(atom("ping"), value);
            }
        },
        others() >> []()
        {
            cout << __FILE__ << " line " << __LINE__ << ": "
                 << to_string(self->last_dequeued()) << endl;
        }
    )
    .until(gref(s_pongs) == num_pings);
    cout << "ping is done" << endl;
}

void pong(actor_ptr ping_actor)
{
    // kickoff
    send(ping_actor, atom("pong"), 0);
    receive_loop
    (
        on<atom("ping"), int>() >> [](int value)
        {
//cout << to_string(self->last_dequeued()) << endl;
            reply(atom("pong"), value + 1);
        },
        others() >> []()
        {
            cout << __FILE__ << " line " << __LINE__ << ": "
                 << to_string(self->last_dequeued()) << endl;
        }
    );
}
