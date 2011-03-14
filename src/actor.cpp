#include "cppa/actor.hpp"

#include <boost/thread.hpp>

namespace cppa {

actor& actor::operator=(const actor& other)
{
	static_cast<super&>(*this) = other;
	return *this;
}

actor& actor::operator=(actor&& other)
{
	static_cast<super&>(*this) = other;
	return *this;
}

actor::~actor()
{
/*
	if (m_channel && m_channel->unique())
	{
		detail::actor_private* ap = dynamic_cast<detail::actor_private*>(m_channel.get());
		if (ap && ap->m_thread)
		{
			if (boost::this_thread::get_id() != ap->m_thread->get_id())
			{
				ap->m_thread->join();
				delete ap->m_thread;
				ap->m_thread = 0;
			}
		}
	}
*/
}

} // namespace cppa
