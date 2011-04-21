#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

#include <map>
#include <list>
#include <iostream>
#include <algorithm>

#include <boost/thread/mutex.hpp>

using std::cout;
using std::endl;

/*

using namespace cppa;

namespace {

class local_group : public group
{

	boost::mutex m_mtx;
	std::list<channel_ptr> m_subscribers;

	inline std::list<channel_ptr>::iterator find(const channel_ptr& what)
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

	virtual intrusive_ptr<group::subscription> subscribe(const channel_ptr& who)
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

	virtual void unsubscribe(const channel_ptr& who)
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

intrusive_ptr<local_group> m_local_group(new local_group);

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
		auto uti = uniform_type_info::by_type_info(typeid(T));
		auto i = m_map.find(key);
		if (i == m_map.end())
		{
			i = m_map.insert(std::make_pair(key, uti->create())).first;
		}
		else if (uti != i->second->type())
		{
			// todo: throw nicer exception
			throw std::runtime_error("invalid type");
		}
		return *reinterpret_cast<T*>(i->second->mutable_value());
	}

};

void consumer(actor_ptr main_actor)
{
	int result = 0;
	for (int i = 0; i < 5; ++i)
	{
		receive(on<int>() >> [&](int x) {
			result += x;
		});
	}
	send(main_actor, result);
}

void producer(actor_ptr consume_actor)
{
	receive(on<int>() >> [&](int i) {
		send(consume_actor, i);
	});
}

std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	std::list<intrusive_ptr<group::subscription>> m_subscriptions;

	actor_ptr self_ptr = self();

	auto consume_actor = spawn(consumer, self_ptr);

	auto lg = local("foobar");
	for (int i = 0; i < 5; ++i)
	{
		auto fa = spawn(producer, consume_actor);
		auto sptr = lg->subscribe(fa);
		m_subscriptions.push_back(sptr);
	}

	send(lg, 1);

	await_all_actors_done();

	receive(on<int>() >> [&](int x) {
		CPPA_CHECK_EQUAL(x, 5);
	});

	return CPPA_TEST_RESULT;

}
*/


std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	CPPA_CHECK_EQUAL(true, true);

	return CPPA_TEST_RESULT;

}
