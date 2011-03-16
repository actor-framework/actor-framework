#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/on.hpp"
#include "cppa/spawn.hpp"
#include "cppa/reply.hpp"
#include "cppa/actor.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include <map>
#include <list>
#include <iostream>
#include <algorithm>

#include <boost/thread/mutex.hpp>

using std::cout;
using std::endl;

using namespace cppa;

namespace {

struct group : detail::channel
{

	// NOT thread safe
	class subscription : public detail::ref_counted_impl<std::size_t>
	{

		actor m_self;
		intrusive_ptr<group> m_group;

	 public:

		subscription() = delete;
		subscription(const subscription&) = delete;
		subscription& operator=(const subscription&) = delete;

		subscription(const actor& s, const intrusive_ptr<group>& g)
			: m_self(s), m_group(g)
		{
		}

		~subscription()
		{
			m_group->unsubscribe(m_self);
		}

	};

	virtual intrusive_ptr<subscription> subscribe(const actor& who) = 0;

	virtual void unsubscribe(const actor& who) = 0;

	template<typename... Args>
	void send(const Args&... args)
	{
		enqueue_msg(message(this_actor(), this, args...));
	}

};

class local_group : public group
{

	boost::mutex m_mtx;
	std::list<actor> m_subscribers;

	inline std::list<actor>::iterator find(const actor& what)
	{
		return std::find(m_subscribers.begin(), m_subscribers.end(), what);
	}

 public:

	virtual void enqueue_msg(const message& msg)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
		{
			i->enqueue_msg(msg);
		}
	}

	virtual intrusive_ptr<group::subscription> subscribe(const actor& who)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		auto i = find(who);
		if (i == m_subscribers.end())
		{
			m_subscribers.push_back(who);
			return new group::subscription(who, this);
		}
		return new group::subscription(0, 0);
	}

	virtual void unsubscribe(const actor& who)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		auto i = find(who);
		if (i != m_subscribers.end())
		{
			m_subscribers.erase(i);
		}
	}

};

//local_group* m_local_group = new local_group;

local_group m_local_group;

group& local(const char*)
{
	return m_local_group;
}

} // namespace <anonymous>

struct storage
{

	std::map<std::string, intrusive_ptr<object>> m_map;

 public:

	template<typename T>
	T& get(const std::string& key)
	{
		const utype& uti = uniform_type_info<T>();
		auto i = m_map.find(key);
		if (i == m_map.end())
		{
			i = m_map.insert(std::make_pair(key, uti.create())).first;
		}
		else if (uti != i->second->type())
		{
			// todo: throw nicer exception
			throw std::runtime_error("invalid type");
		}
		return *reinterpret_cast<T*>(i->second->mutable_value());
	}

};

/*
template<typename... Args>
void send(std::list<actor>& actors, const Args&... args)
{
	for (auto i = actors.begin(); i != actors.end(); ++i)
	{
		i->send(args...);
	}
}
*/

void foo_actor()
{
	receive(on<int>() >> [](int i) {
		reply(i);
	});
}

std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	std::list<intrusive_ptr<group::subscription>> m_subscriptions;

	group& lg = local("foobar");
	for (int i = 0; i < 5; ++i)
	{
		m_subscriptions.push_back(lg.subscribe(spawn(foo_actor)));
	}

	lg.send(1);

	int result = 0;

	for (int i = 0; i < 5; ++i)
	{
		receive(on<int>() >> [&result](int x) {
			result += x;
		});
	}

	CPPA_CHECK_EQUAL(result, 5);

	return CPPA_TEST_RESULT;

}
