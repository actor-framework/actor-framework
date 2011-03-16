#ifndef ACTOR_PRIVATE_HPP
#define ACTOR_PRIVATE_HPP

#include <boost/thread/thread.hpp>

#include "cppa/invoke_rules.hpp"
#include "cppa/untyped_tuple.hpp"

#include "cppa/detail/channel.hpp"
#include "cppa/detail/actor_public.hpp"

namespace cppa { class message; }

namespace cppa { namespace detail {

// private part of the actor interface (callable only from this_actor())
struct actor_private : public actor_public
{
	boost::thread* m_thread;
	inline actor_private() : m_thread(0) { }
	virtual ~actor_private();
	virtual const message& receive() = 0;
	virtual const message& last_dequeued() const = 0;
	virtual void receive(invoke_rules&) = 0;
	virtual void send(channel* whom, untyped_tuple&& what) = 0;
};

} } // namespace cppa::detail

namespace cppa {

detail::actor_private* this_actor();

inline const message& receive()
{
	return this_actor()->receive();
}

inline void receive(invoke_rules& rules)
{
	this_actor()->receive(rules);
}

inline void receive(invoke_rules&& rules)
{
	invoke_rules tmp(std::move(rules));
	this_actor()->receive(tmp);
}

inline const message& last_dequeued()
{
	return this_actor()->last_dequeued();
}

} // namespace cppa

#endif // ACTOR_PRIVATE_HPP
