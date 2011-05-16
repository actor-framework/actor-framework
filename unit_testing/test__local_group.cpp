#include "cppa/config.hpp"

#include <map>
#include <list>
#include <mutex>
#include <iostream>
#include <algorithm>

#include "test.hpp"
#include "hash_of.hpp"

#include "cppa/on.hpp"
#include "cppa/cppa.hpp"
#include "cppa/actor.hpp"
#include "cppa/group.hpp"
#include "cppa/ref_counted.hpp"
#include "cppa/intrusive_ptr.hpp"

using std::cout;
using std::endl;

using namespace cppa;

class local_group : public group
{

    friend class local_group_module;

    std::mutex m_mtx;
    std::list<channel_ptr> m_subscribers;

    inline std::list<channel_ptr>::iterator find(const channel_ptr& what)
    {
        return std::find(m_subscribers.begin(), m_subscribers.end(), what);
    }

    local_group() = default;

 public:

    virtual void enqueue(const message& msg)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
        {
            (*i)->enqueue(msg);
        }
    }

    virtual group::subscription subscribe(const channel_ptr& who)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        auto i = find(who);
        if (i == m_subscribers.end())
        {
            m_subscribers.push_back(who);
            return { who, this };
        }
        return { nullptr, nullptr };
    }

    virtual void unsubscribe(const channel_ptr& who)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        auto i = find(who);
        if (i != m_subscribers.end())
        {
            m_subscribers.erase(i);
        }
    }

};

class local_group_module : public group::module
{

    std::string m_name;
    std::mutex m_mtx;
    std::map<std::string, group_ptr> m_instances;


 public:

    local_group_module() : m_name("local")
    {
    }

    const std::string& name()
    {
        return m_name;
    }

    group_ptr get(const std::string& group_name)
    {
        std::lock_guard<std::mutex> guard(m_mtx);
        auto i = m_instances.find(group_name);
        if (i == m_instances.end())
        {
            group_ptr result(new local_group);
            m_instances.insert(std::make_pair(group_name, result));
            return result;
        }
        else return i->second;
    }

};

void worker()
{
    receive(on<int>() >> [](int value) {
        reply(value);
    });
}

size_t test__local_group()
{
    CPPA_TEST(test__local_group);
    group::add_module(new local_group_module);
    auto foo_group = group::get("local", "foo");
    for (int i = 0; i < 5; ++i)
    {
        // spawn workers in group local:foo
        spawn(worker)->join(foo_group);
    }
    send(foo_group, 2);
    int result = 0;
    for (int i = 0; i < 5; ++i)
    {
        receive(on<int>() >> [&result](int value) { result += value; });
    }
    CPPA_CHECK_EQUAL(result, 10);
    await_all_others_done();
    return CPPA_TEST_RESULT;
}
