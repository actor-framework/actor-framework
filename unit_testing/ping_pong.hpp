#ifndef PING_PONG_HPP
#define PING_PONG_HPP

#include "cppa/actor.hpp"

void ping();
void pong(cppa::actor_ptr ping_actor);

// returns the number of messages ping received
int pongs();

#endif // PING_PONG_HPP
