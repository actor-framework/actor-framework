#include <boost/thread.hpp>

#include "cppa/detail/scheduler.hpp"

#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"

namespace {

struct actor_message
{
	actor_message* next;
	cppa::message msg;
	actor_message(const cppa::message& from) : msg(from) { }
};

struct actor_impl;

void cleanup_fun(actor_impl* what);

boost::thread_specific_ptr<actor_impl> m_this_actor(cleanup_fun);

struct actor_impl : cppa::detail::actor_private
{

	cppa::message m_last_dequeued;

	cppa::util::single_reader_queue<actor_message> mailbox;

	cppa::detail::behavior* m_behavior;

	actor_impl(cppa::detail::behavior* b = 0) : m_behavior(b) { }

	virtual void enqueue_msg(const cppa::message& msg)
	{
		mailbox.push_back(new actor_message(msg));
	}

	virtual const cppa::message& receive()
	{
		actor_message* msg = mailbox.pop();
		m_last_dequeued = std::move(msg->msg);
		delete msg;
		return m_last_dequeued;
	}

	virtual const cppa::message& last_dequeued() const
	{
		return m_last_dequeued;
	}

	virtual void receive(cppa::invoke_rules& rules)
	{
		actor_message* amsg = mailbox.pop();
		cppa::util::singly_linked_list<actor_message> buffer;
		cppa::intrusive_ptr<cppa::detail::intermediate> imd;
		while (!(imd = rules.get_intermediate(amsg->msg.data())))
		{
			buffer.push_back(amsg);
			amsg = mailbox.pop();
		}
		m_last_dequeued = amsg->msg;
		if (!buffer.empty()) mailbox.prepend(std::move(buffer));
		imd->invoke();
		delete amsg;
	}

	virtual void send(cppa::detail::channel* whom,
					  cppa::untyped_tuple&& what)
	{
		if (whom) whom->enqueue_msg(cppa::message(this, whom, std::move(what)));
	}

	void operator()()
	{
		if (m_behavior)
		{
			try
			{
				m_behavior->act();
			}
			catch (...) { }
			m_behavior->on_exit();
		}
	}

};

void cleanup_fun(actor_impl* what)
{
	if (what)
	{
		if (!what->deref()) delete what;
	}
}

struct actor_ptr
{
	cppa::intrusive_ptr<actor_impl> m_impl;
	actor_ptr(actor_impl* ai) : m_impl(ai) { }
	actor_ptr(actor_ptr&& other) : m_impl(std::move(other.m_impl)) { }
	actor_ptr(const actor_ptr&) = default;
	void operator()()
	{
		m_this_actor.reset(m_impl.get());
		(*m_impl)();
	}
};

} // namespace <anonymous>

namespace cppa {

detail::actor_private* this_actor()
{
	actor_impl* res = m_this_actor.get();
	if (!res)
	{
		res = new actor_impl;
		res->ref();
		m_this_actor.reset(res);
	}
	return res;
}

} // namespace cppa

namespace cppa { namespace detail {

actor spawn_impl(behavior* actor_behavior)
{
	actor_ptr aptr(new actor_impl(actor_behavior));
	boost::thread(aptr).detach();
	return actor(std::move(aptr.m_impl));
}

} } // namespace cppa::detail
