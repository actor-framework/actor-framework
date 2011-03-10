#include "test.hpp"

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

struct
{
	intrusive_ptr<group> operator/(const std::string& group_name)
	{
		return m_groups[group_name];
	}
}
local;

void foo_actor()
{
	auto x = (local/"foobar")->subscribe(this_actor());
	receive(on<int, int, int>() >> []() {
		reply(23.f);
	});
	receive(on<int>() >> [](int i) {
		reply(i);
	});
}

std::size_t test__local_group()
{

	CPPA_TEST(test__local_group);

	auto g = local/"foobar";

	for (int i = 0; i < 5; ++i)
	{
		spawn(foo_actor).send(1, 2, 3);
	}

	for (int i = 0; i < 5; ++i)
	{
		receive(on<float>() >> [&]() { });
	}

	g->send(1);

	int result = 0;
	for (int i = 0; i < 5; ++i)
	{
		receive(on<int>() >> [&](int x) {
			result += x;
		});
	}

	CPPA_CHECK_EQUAL(result, 5);

	return CPPA_TEST_RESULT;

}
