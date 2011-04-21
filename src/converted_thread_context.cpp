#include "cppa/detail/converted_thread_context.hpp"

namespace cppa { namespace detail {

void converted_thread_context::link(intrusive_ptr<actor>& other)
{
	std::lock_guard<std::mutex> guard(m_mtx);
	if (other && !m_exited && other->establish_backlink(this))
	{
		m_links.insert(other);
	}
}

bool converted_thread_context::remove_backlink(const intrusive_ptr<actor>& other)
{
	if (other && other != this)
	{
		std::lock_guard<std::mutex> guard(m_mtx);
		return m_links.erase(other) > 0;
	}
	return false;
}


bool converted_thread_context::establish_backlink(const intrusive_ptr<actor>& other)
{
	if (other && other != this)
	{
		std::lock_guard<std::mutex> guard(m_mtx);
		return m_links.insert(other).second;
	}
	return false;
}

void converted_thread_context::unlink(intrusive_ptr<actor>& other)
{
	std::lock_guard<std::mutex> guard(m_mtx);
	if (other && !m_exited && other->remove_backlink(this))
	{
		m_links.erase(other);
	}
}

void converted_thread_context::join(group_ptr& what)
{
	std::lock_guard<std::mutex> guard(m_mtx);
	if (!m_exited && m_subscriptions.count(what) == 0)
	{
		m_subscriptions[what] = what->subscribe(this);
	}
}

void converted_thread_context::leave(const group_ptr& what)
{
	std::lock_guard<std::mutex> guard(m_mtx);
	m_subscriptions.erase(what);
}

message_queue& converted_thread_context::mailbox()
{
	return m_mailbox;
}

} } // namespace cppa::detail
