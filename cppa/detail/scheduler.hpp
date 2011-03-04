#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "cppa/actor.hpp"
#include "cppa/message.hpp"

namespace cppa { namespace detail {

// public part of the actor interface (callable from other actors)
struct actor_public
{
	virtual void enqueue_msg(const message& msg) = 0;
};

// private part of the actor interface (callable only from this_actor())
struct actor_private : public actor_public
{
	virtual message dequeue_msg() = 0;

};

actor_private* this_actor();

actor spawn_impl();

} } // namespace cppa::detail

#endif // SCHEDULER_HPP
