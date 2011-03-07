#ifndef ACTOR_HPP
#define ACTOR_HPP

#include "cppa/tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/detail/actor_public.hpp"
#include "cppa/detail/actor_private.hpp"

namespace cppa {

class actor
{

 public:

	typedef cppa::intrusive_ptr<detail::actor_public> ptr_type;

	actor() = default;

	inline actor(detail::actor_public* ptr) : m_ptr(ptr) { }

	inline actor(const ptr_type& ptr) : m_ptr(ptr) { }

	inline actor(ptr_type&& ptr) : m_ptr(ptr) { }

	actor(const actor&) = default;

	inline actor(actor&& other) : m_ptr(std::move(other.m_ptr)) { }

	actor& operator=(const actor&) = default;

	actor& operator=(actor&& other)
	{
		m_ptr = std::move(other.m_ptr);
		return *this;
	}

	template<typename... Args>
	void send(const Args&... args)
	{
		this_actor()->send(m_ptr.get(), tuple<Args...>(args...));
	}

	template<typename... Args>
	void send_tuple(const tuple<Args...>& args)
	{
		this_actor()->send(m_ptr.get(), args);
	}

 private:

	ptr_type m_ptr;

};

/*
template<typename... Args>
actor& operator<<(actor& whom, const tuple<Args...>& data)
{
	whom.send_tuple(data);
}
*/

} // namespace cppa

#endif // ACTOR_HPP
