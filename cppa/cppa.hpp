#ifndef CPPA_HPP
#define CPPA_HPP

#include "cppa/actor.hpp"
#include "cppa/channel.hpp"
#include "cppa/context.hpp"
#include "cppa/message.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/actor_behavior.hpp"
#include "cppa/scheduling_hint.hpp"
#include "cppa/detail/scheduler.hpp"

namespace cppa {

template<typename F>
actor_ptr spawn(F fun, scheduling_hint hint = scheduled)
{
	struct fun_behavior : actor_behavior
	{
		F m_fun;
		fun_behavior(const F& fun_arg) : m_fun(fun_arg) { }
		virtual void act()
		{
			m_fun();
		}
	};
	return detail::scheduler::spawn(new fun_behavior(fun), hint);
}

inline actor_ptr spawn(actor_behavior* ab, scheduling_hint hint = scheduled)
{
	return detail::scheduler::spawn(ab, hint);
}

inline context* self()
{
	return detail::scheduler::get_context();
}

inline const message& receive()
{
	return self()->mailbox().dequeue();
}

inline void receive(invoke_rules& rules)
{
	self()->mailbox().dequeue(rules);
}

inline void receive(invoke_rules&& rules)
{
	self()->mailbox().dequeue(rules);
}

inline bool try_receive(message& msg)
{
	return self()->mailbox().try_dequeue(msg);
}

inline bool try_receive(invoke_rules& rules)
{
	return self()->mailbox().try_dequeue(rules);
}

inline const message& last_received()
{
	return self()->mailbox().last_dequeued();
}

template<typename Arg0, typename... Args>
void send(channel_ptr whom, const Arg0& arg0, const Args&... args)
{
	if (whom) whom->enqueue(message(self(), whom, arg0, args...));
}

template<typename Arg0, typename... Args>
void reply(const Arg0& arg0, const Args&... args)
{
	context* sptr = self();
	actor_ptr whom = sptr->mailbox().last_dequeued().sender();
	if (whom) whom->enqueue(message(sptr, whom, arg0, args...));
}

inline void await_all_actors_done()
{
	detail::scheduler::await_all_done();
}

} // namespace cppa

#endif // CPPA_HPP
