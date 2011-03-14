#include "cppa/message_receiver.hpp"

namespace cppa {

message_receiver& message_receiver::operator=(detail::channel* ch_ptr)
{
	m_channel.reset(ch_ptr);
	return *this;
}

message_receiver&
message_receiver::operator=(const intrusive_ptr<detail::channel>& ch_ptr)
{
	m_channel = ch_ptr;
	return *this;
}

message_receiver&
message_receiver::operator=(intrusive_ptr<detail::channel>&& ch_ptr)
{
	m_channel.swap(ch_ptr);
	return *this;
}

message_receiver::~message_receiver()
{
}

} // namespace cppa
