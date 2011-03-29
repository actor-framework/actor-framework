#include "cppa/group.hpp"

namespace cppa {

group::subscription::subscription(const channel_ptr& s, const intrusive_ptr<group>& g) : m_self(s), m_group(g)
{
}

group::subscription::~subscription()
{
	m_group->unsubscribe(m_self);
}

} // namespace cppa
