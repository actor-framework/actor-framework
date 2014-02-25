#ifndef PING_PONG_HPP
#define PING_PONG_HPP

//#include "cppa/actor.hpp"

#include <cstddef>
#include "cppa/cppa_fwd.hpp"

void ping(size_t num_pings);

void event_based_ping(size_t num_pings);

void pong(cppa::actor_ptr ping_actor);

void event_based_pong(cppa::actor_ptr ping_actor);

// returns the number of messages ping received
size_t pongs();

#endif // PING_PONG_HPP
