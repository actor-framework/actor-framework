#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include "cppa/actor.hpp"
#include "cppa/tuple.hpp"
#include "cppa/untyped_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"

namespace cppa {

class message
{

 public:

	struct content : ref_counted
	{
		const actor sender;
		const actor receiver;
		const untyped_tuple data;
		content(const actor& s, const actor& r, const untyped_tuple& ut)
			: sender(s), receiver(r), data(ut)
		{
		}
		content(const actor& s, const actor& r, untyped_tuple&& ut)
			: sender(s), receiver(r), data(ut)
		{
		}
	};

 private:

	intrusive_ptr<content> m_content;

 public:

	template<typename... Args>
	message(const actor& from, const actor& to, const Args&... args)
		: m_content(new content(from, to, tuple<Args...>(args...))) { }

	message(const actor& from, const actor& to, const untyped_tuple& ut)
		: m_content(new content(from, to, ut)) { }

	message(const actor& from, const actor& to, untyped_tuple&& ut)
		: m_content(new content(from, to, std::move(ut))) { }

	message() : m_content(new content(actor(), actor(), tuple<int>(0))) { }

	const actor& sender() const
	{
		return m_content->sender;
	}
	const actor& receiver() const
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
