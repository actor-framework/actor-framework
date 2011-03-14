#ifndef MESSAGE_RECEIVER_HPP
#define MESSAGE_RECEIVER_HPP

#include "cppa/tuple.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/channel.hpp"
#include "cppa/detail/comparable.hpp"
#include "cppa/detail/actor_private.hpp"

namespace cppa {

class message_receiver :
		detail::comparable<message_receiver, intrusive_ptr<detail::channel>>,
		detail::comparable<message_receiver, message_receiver>
{

 protected:

	intrusive_ptr<detail::channel> m_channel;

 public:

	inline message_receiver(detail::channel* ch_ptr) : m_channel(ch_ptr) { }

	inline message_receiver(const intrusive_ptr<detail::channel>& ch_ptr)
		: m_channel(ch_ptr)
	{
	}

	inline message_receiver(intrusive_ptr<detail::channel>&& ch_ptr)
		: m_channel(std::move(ch_ptr))
	{
	}

	virtual ~message_receiver();

	message_receiver() = default;

	message_receiver(message_receiver&&) = default;

	message_receiver(const message_receiver&) = default;

	message_receiver& operator=(const message_receiver&) = default;

	message_receiver& operator=(detail::channel*);

	message_receiver& operator=(const intrusive_ptr<detail::channel>&);

	message_receiver& operator=(intrusive_ptr<detail::channel>&&);

	void enqueue_msg(const message& msg)
	{
		m_channel->enqueue_msg(msg);
	}

	template<typename... Args>
	void send(const Args&... args)
	{
		this_actor()->send(m_channel.get(), tuple<Args...>(args...));
	}

	inline bool equal_to(const intrusive_ptr<detail::channel>& ch_ptr) const
	{
		return m_channel == ch_ptr;
	}

	inline bool equal_to(const message_receiver& other) const
	{
		return m_channel == other.m_channel;
	}

};

} // namespace cppa

#endif // MESSAGE_RECEIVER_HPP
