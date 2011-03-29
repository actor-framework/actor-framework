#include <set>
#include <map>

#include <boost/thread.hpp>

#include "cppa/message.hpp"
#include "cppa/context.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/detail/scheduler.hpp"

#include "cppa/util/singly_linked_list.hpp"
#include "cppa/util/single_reader_queue.hpp"

using namespace cppa;

namespace {

struct actor_message
{
	actor_message* next;
	message msg;
	actor_message(const message& from) : next(0), msg(from) { }
};

struct actor_impl;

void cleanup_fun(context* what);

boost::thread_specific_ptr<context> m_this_context(cleanup_fun);

struct mbox : message_queue
{

	message m_last_dequeued;
	util::single_reader_queue<actor_message> m_impl;

	void enqueue(const message& msg)
	{
		m_impl.push_back(new actor_message(msg));
	}

	virtual const message& dequeue()
	{
		actor_message* msg = m_impl.pop();
		m_last_dequeued = std::move(msg->msg);
		delete msg;
		return m_last_dequeued;
	}

	virtual void dequeue(invoke_rules& rules)
	{
		actor_message* amsg = m_impl.pop();
		util::singly_linked_list<actor_message> buffer;
		intrusive_ptr<detail::intermediate> imd;
		while (!(imd = rules.get_intermediate(amsg->msg.data())))
		{
			buffer.push_back(amsg);
			amsg = m_impl.pop();
		}
		m_last_dequeued = amsg->msg;
		if (!buffer.empty()) m_impl.push_front(std::move(buffer));
		imd->invoke();
		delete amsg;
	}

	virtual bool try_dequeue(message& msg)
	{
		if (!m_impl.empty())
		{
			msg = dequeue();
			return true;
		}
		return false;
	}

	virtual bool try_dequeue(invoke_rules& rules)
	{
		if (!m_impl.empty())
		{
			dequeue(rules);
			return true;
		}
		return false;
	}

	virtual const message& last_dequeued()
	{
		return m_last_dequeued;
	}

};

struct actor_impl : context
{

	actor_behavior* m_behavior;
	bool m_exited;

	// guards m_exited, m_grp_lst and m_links
	boost::mutex m_mtx;
	std::map<group_ptr, intrusive_ptr<group::subscription> > m_grp_lst;
	std::set<actor_ptr> m_links;

	mbox m_mbox;

	actor_impl(actor_behavior* b = 0) : m_behavior(b), m_exited(false) { }

	~actor_impl()
	{
		if (m_behavior) delete m_behavior;
	}

	virtual void enqueue(const message& msg)
	{
		m_mbox.enqueue(msg);
	}

	virtual void link(intrusive_ptr<actor>& other)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		if (other && !m_exited && other->establish_backlink(this))
		{
			m_links.insert(other);
		}
	}

	virtual bool remove_backlink(const intrusive_ptr<actor>& other)
	{
		if (other && other != this)
		{
			boost::mutex::scoped_lock guard(m_mtx);
			return m_links.erase(other) > 0;
		}
		return false;
	}


	virtual bool establish_backlink(const intrusive_ptr<actor>& other)
	{
		if (other && other != this)
		{
			boost::mutex::scoped_lock guard(m_mtx);
			return m_links.insert(other).second;
		}
		return false;
	}

	virtual void unlink(intrusive_ptr<actor>& other)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		if (other && !m_exited && other->remove_backlink(this))
		{
			m_links.erase(other);
		}
	}

	virtual void join(group_ptr& what)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		if (!m_exited && m_grp_lst.count(what) == 0)
		{
			m_grp_lst[what] = what->subscribe(this);
		}
	}

	virtual void leave(const group_ptr& what)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		m_grp_lst.erase(what);
	}

	virtual message_queue& mailbox()
	{
		return m_mbox;
	}

};

void cleanup_fun(context* what)
{
	if (what && !what->deref()) delete what;
}

std::atomic<int> m_running_actors(0);
boost::mutex m_ra_mtx;
boost::condition_variable m_ra_cv;

void run_actor_impl(intrusive_ptr<actor_impl> m_impl)
{
	{
		actor_impl* self_ptr = m_impl.get();
		self_ptr->ref();
		m_this_context.reset(self_ptr);
	}
	actor_behavior* ab = m_impl->m_behavior;
	if (ab)
	{
		try
		{
			ab->act();
		}
		catch(...) { }
		ab->on_exit();
	}
	if (--m_running_actors == 0)
	{
		boost::mutex::scoped_lock lock(m_ra_mtx);
		m_ra_cv.notify_all();
	}
}

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_ptr scheduler::spawn(actor_behavior* ab, scheduling_hint)
{
	++m_running_actors;
	intrusive_ptr<actor_impl> result(new actor_impl(ab));
	boost::thread(run_actor_impl, result).detach();
	return result;
}

context* scheduler::get_context()
{
	context* result = m_this_context.get();
	if (!result)
	{
		result = new actor_impl;
		result->ref();
		m_this_context.reset(result);
	}
	return result;
}

void scheduler::await_all_done()
{
	boost::mutex::scoped_lock lock(m_ra_mtx);
	while (m_running_actors.load() > 0)
	{
		m_ra_cv.wait(lock);
	}
}

} } // namespace detail
