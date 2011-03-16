#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/message_receiver.hpp"

#include "cppa/detail/channel.hpp"

namespace cppa {

class message
{

 public:

	struct content : ref_counted
	{
		const actor sender;
		const message_receiver receiver;
		const untyped_tuple data;
		content(const actor& s, const message_receiver& r,
				const untyped_tuple& ut)
			: ref_counted(), sender(s), receiver(r), data(ut)
		{
		}
		~content()
		{
		}
	};

 private:

	intrusive_ptr<content> m_content;

 public:

	template<typename... Args>
	message(const actor& from, const message_receiver& to, const Args&... args)
		: m_content(new content(from, to, tuple<Args...>(args...))) { }

	message(const actor& from, const message_receiver& to, const untyped_tuple& ut)
		: m_content(new content(from, to, ut)) { }

//	message(const actor& from, const message_receiver& to, untyped_tuple&& ut)
//		: m_content(new content(from, to, std::move(ut))) { }

	message() : m_content(new content(0, 0, tuple<int>(0))) { }

	const actor& sender() const
	{
		return m_content->sender;
	}
	const message_receiver& receiver() const
	{
		return m_content->receiver;
	}

	const untyped_tuple& data() const
	{
		return m_content->data;
	}

};

} // namespace cppa

#include "cppa/reply.hpp"

#endif // MESSAGE_HPP
