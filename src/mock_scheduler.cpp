#include "cppa/config.hpp"

#include <set>
#include <map>
#include <thread>

// for thread_specific_ptr
// needed unless the new keyword "thread_local" works in GCC
#include <boost/thread.hpp>

#include "cppa/message.hpp"
#include "cppa/context.hpp"
#include "cppa/invoke_rules.hpp"
#include "cppa/scheduler.hpp"
#include "cppa/detail/mock_scheduler.hpp"

#include "cppa/detail/converted_thread_context.hpp"

namespace {

std::atomic<int> m_running_actors(0);
boost::mutex m_ra_mtx;
boost::condition_variable m_ra_cv;

void run_actor(cppa::intrusive_ptr<cppa::context> m_self,
			   cppa::actor_behavior* behavior)
{
	cppa::set_self(m_self.get());
	if (behavior)
	{
		try
		{
			behavior->act();
		}
		catch(...) { }
		behavior->on_exit();
	}
	if (--m_running_actors == 0)
	{
		boost::mutex::scoped_lock lock(m_ra_mtx);
		m_ra_cv.notify_all();
	}
}

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_ptr mock_scheduler::spawn(actor_behavior* ab, scheduling_hint)
{
	++m_running_actors;
	intrusive_ptr<context> ctx(new detail::converted_thread_context);
	boost::thread(run_actor, ctx, ab).detach();
	return ctx;
}

void mock_scheduler::await_all_done()
{
	boost::mutex::scoped_lock lock(m_ra_mtx);
	while (m_running_actors.load() > 0)
	{
		m_ra_cv.wait(lock);
	}
}

} } // namespace detail
