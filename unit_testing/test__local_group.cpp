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

class group : public detail::channel
{

	boost::mutex m_mtx;
	std::list<actor> m_subscribers;

 public:

	struct subscription
	{
		actor m_self;
		intrusive_ptr<group> m_group;
		subscription(const actor& s, const intrusive_ptr<group>& g)
			: m_self(s), m_group(g)
		{
		}
		~subscription()
		{
			m_group->unsubscribe(m_self);
		}
	};

	subscription subscribe(const actor& who)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		m_subscribers.push_back(who);
		return { who, this };
	}

	void unsubscribe(const actor& who)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		auto i = std::find(m_subscribers.begin(), m_subscribers.end(), who);
		if (i != m_subscribers.end())
		{
			m_subscribers.erase(i);
		}
	}

	void enqueue_msg(const message& msg)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
		{
			i->enqueue_msg(msg);
		}
	}

	template<typename... Args>
	void send(const Args&... args)
	{
		message msg(this_actor(), this, args...);
		enqueue_msg(msg);
	}

};

namespace {

class group_bucket
{

	boost::mutex m_mtx;
	std::map<std::string, intrusive_ptr<group>> m_groups;

 public:

	intrusive_ptr<group> get(const std::string& group_name)
	{
		boost::mutex::scoped_lock guard(m_mtx);
		intrusive_ptr<group>& result = m_groups[group_name];
		if (!result)
		{
			result.reset(new group);
		}
		return result;
	}

};

template<std::size_t NumBuckets>
class group_table
{

	group_bucket m_buckets[NumBuckets];

	group_bucket& bucket(const std::string& group_name)
	{
		unsigned int gn_hash = hash_of(group_name);
		return m_buckets[gn_hash % NumBuckets];
	}

 public:

	intrusive_ptr<group> operator[](const std::string& group_name)
	{
		return bucket(group_name).get(group_name);
	}

};

group_table<100> m_groups;

} // namespace <anonymous>


namespace {

intrusive_ptr<group> local_group = new group;

}

struct
{
	intrusive_ptr<group> operator/(const std::string& /*group_name*/)
	{
		return local_group;
//		return m_groups[group_name];
	}
}
local;

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

template<typename... Args>
void send(std::list<actor>& actors, const Args&... args)
{
	for (auto i = actors.begin(); i != actors.end(); ++i)
	{
		i->send(args...);
	}
}

void foo_actor()
{
//	auto x = (local/"foobar")->subscribe(this_actor());
	auto rules = (
		on<int, int, int>() >> []() {
			reply(23.f);
		},
		on<int>() >> [](int i) {
			reply(i);
		}
	);
	receive(rules);
	receive(rules);
//	reply(1);
}

std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	/*
	storage st;

	st.get<int>("foobaz") = 42;

	CPPA_CHECK_EQUAL(st.get<int>("foobaz"), 42);

	st.get<std::string>("_s") = "Hello World!";

	CPPA_CHECK_EQUAL(st.get<std::string>("_s"), "Hello World!");

	*/

//	auto g = local/"foobar";

	std::list<actor> m_slaves;

	for (int i = 0; i < 5; ++i)
	{
		m_slaves.push_back(spawn(foo_actor));
	}

	send(m_slaves, 1, 2, 3);
	send(m_slaves, 1);

	for (int i = 0; i < 5; ++i)
	{
		receive(on<float>() >> []() { });
	}

//	g->send(1);

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
