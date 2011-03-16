#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
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

struct group : channel
{

	// NOT thread safe
	class subscription : public detail::ref_counted_impl<std::size_t>
	{

		actor_ptr m_self;
		intrusive_ptr<group> m_group;

	 public:

		subscription() = delete;
		subscription(const subscription&) = delete;
		subscription& operator=(const subscription&) = delete;

		subscription(const actor_ptr& s, const intrusive_ptr<group>& g)
			: m_self(s), m_group(g)
		{
		}

		~subscription()
		{
			m_group->unsubscribe(m_self);
		}

	};

	virtual intrusive_ptr<subscription> subscribe(const actor_ptr& who) = 0;

	virtual void unsubscribe(const actor_ptr& who) = 0;

};

class local_group : public group
{

	boost::mutex m_mtx;
	std::list<actor_ptr> m_subscribers;

	inline std::list<actor_ptr>::iterator find(const actor_ptr& what)
	{
		return std::find(m_subscribers.begin(), m_subscribers.end(), what);
	}

 public:

	virtual void enqueue(const message& msg)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
		{
			(*i)->enqueue(msg);
		}
	}

	virtual intrusive_ptr<group::subscription> subscribe(const actor_ptr& who)
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

	virtual void unsubscribe(const actor_ptr& who)
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

intrusive_ptr<local_group> m_local_group;

intrusive_ptr<local_group> local(const char*)
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

void foo_actor_ptr()
{
	receive(on<int>() >> [](int i) {
		reply(i);
	});
}

std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	std::list<intrusive_ptr<group::subscription>> m_subscriptions;

	auto lg = local("foobar");
	for (int i = 0; i < 5; ++i)
	{
		m_subscriptions.push_back(lg->subscribe(spawn(foo_actor_ptr)));
	}

	send(lg, 1);

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
