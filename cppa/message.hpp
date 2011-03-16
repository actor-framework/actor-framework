#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/channel.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"

#include "cppa/detail/channel.hpp"

namespace cppa {

class message
{

 public:

	struct content : ref_counted
	{
		const actor_ptr sender;
		const channel_ptr receiver;
		const untyped_tuple data;
		content(const actor_ptr& s, const channel_ptr& r,
				const untyped_tuple& ut)
			: ref_counted(), sender(s), receiver(r), data(ut)
		{
		}
	};

 private:

	intrusive_ptr<content> m_content;

 public:

	template<typename... Args>
	message(const actor_ptr& from, const channel_ptr& to, const Args&... args)
		: m_content(new content(from, to, tuple<Args...>(args...))) { }

	message(const actor_ptr& from, const channel_ptr& to, const untyped_tuple& ut)
		: m_content(new content(from, to, ut)) { }

//	message(const actor_ptr& from, const channel_ptr& to, untyped_tuple&& ut)
//		: m_content(new content(from, to, std::move(ut))) { }

	message() : m_content(new content(0, 0, tuple<int>(0))) { }

	const actor_ptr& sender() const
	{
		return m_content->sender;
	}
	const channel_ptr& receiver() const
	{
		return m_content->receiver;
	}

	const untyped_tuple& data() const
	{
		return m_content->data;
	}

};

} // namespace cppa

#endif // MESSAGE_HPP
