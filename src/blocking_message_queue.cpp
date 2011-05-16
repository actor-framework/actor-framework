#include "cppa/invoke_rules.hpp"

#include "cppa/util/singly_linked_list.hpp"

#include "cppa/detail/intermediate.hpp"
#include "cppa/detail/blocking_message_queue.hpp"

namespace cppa { namespace detail {

void blocking_message_queue::enqueue(const message& msg)
{
	m_queue.push_back(new queue_node(msg));
}

const message& blocking_message_queue::dequeue()
{
	queue_node* msg = m_queue.pop();
	m_last_dequeued = std::move(msg->msg);
	delete msg;
	return m_last_dequeued;
}

void blocking_message_queue::dequeue(invoke_rules& rules)
{
	queue_node* amsg = m_queue.pop();
	util::singly_linked_list<queue_node> buffer;
	intrusive_ptr<detail::intermediate> imd;
	while (!(imd = rules.get_intermediate(amsg->msg.content())))
	{
		buffer.push_back(amsg);
		amsg = m_queue.pop();
	}
	m_last_dequeued = amsg->msg;
	if (!buffer.empty()) m_queue.push_front(std::move(buffer));
	imd->invoke();
	delete amsg;
}

bool blocking_message_queue::try_dequeue(message& msg)
{
	if (!m_queue.empty())
	{
		msg = dequeue();
		return true;
	}
	return false;
}

bool blocking_message_queue::try_dequeue(invoke_rules& rules)
{
	if (!m_queue.empty())
	{
		dequeue(rules);
		return true;
	}
	return false;
}

const message& blocking_message_queue::last_dequeued()
{
	return m_last_dequeued;
}

} } // namespace hamcast::detail
