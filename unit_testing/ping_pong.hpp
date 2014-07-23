#ifndef PING_PONG_HPP
#define PING_PONG_HPP

//#include "caf/actor.hpp"

#include <cstddef>
#include "caf/fwd.hpp"

void ping(caf::blocking_actor*, size_t num_pings);

void event_based_ping(caf::event_based_actor*, size_t num_pings);

void pong(caf::blocking_actor*, caf::actor ping_actor);

void event_based_pong(caf::event_based_actor*, caf::actor ping_actor);

// returns the number of messages ping received
size_t pongs();

#endif // PING_PONG_HPP
