#include "cppa/config.hpp"

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <memory>
#include <stdexcept>

#include "cppa/group.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace cppa {

namespace {

typedef std::map< std::string, std::unique_ptr<group::module> > modules_map;

typedef std::lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

std::mutex s_mtx;
modules_map s_mmap;

class local_group : public group
{

    friend class local_group_module;

    util::shared_spinlock m_mtx;
    std::set<channel_ptr> m_subscribers;

    // allow access to local_group_module only
    local_group(std::string&& gname) : group(std::move(gname), "local")
    {
    }

 public:

    virtual void enqueue(const message& msg)
    {
        shared_guard guard(m_mtx);
        for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
        {
            const_cast<channel_ptr&>(*i)->enqueue(msg);
        }
    }

    virtual group::subscription subscribe(const channel_ptr& who)
    {
        exclusive_guard guard(m_mtx);
        if (m_subscribers.insert(who).second)
        {
            return { who, this };
        }
        else
        {
            return { nullptr, nullptr };
        }
    }

    virtual void unsubscribe(const channel_ptr& who)
    {
        exclusive_guard guard(m_mtx);
        m_subscribers.erase(who);
    }

};

class local_group_module : public group::module
{

    std::string m_name;
    util::shared_spinlock m_mtx;
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
        shared_guard guard(m_mtx);
        auto i = m_instances.find(group_name);
        if (i != m_instances.end())
        {
            return i->second;
        }
        else
        {
            group_ptr tmp(new local_group(std::string(group_name)));
            // lifetime scope of uguard
            {
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(group_name, tmp));
                if (p.second == false)
                {
                    // someone preempt us
                    return p.first->second;
                }
                else
                {
                    // ok, inserted tmp
                    return tmp;
                }
            }
        }
    }

};

modules_map& mmap()
{
    if (s_mmap.empty())
    {
        // lazy initialization
        s_mmap.insert(std::make_pair(std::string("local"),
                                     new local_group_module));
    }
    return s_mmap;
}

} // namespace <anonymous>

intrusive_ptr<group> group::get(const std::string& module_name,
                                const std::string& group_name)
{
    // lifetime scope of guard
    {
        std::lock_guard<std::mutex> guard(s_mtx);
        auto& mmref = mmap();
        auto i = mmref.find(module_name);
        if (i != mmref.end())
        {
            return (i->second)->get(group_name);
        }
    }
    std::string error_msg = "no module named \"";
    error_msg += module_name;
    error_msg += "\" found";
    throw std::logic_error(error_msg);
}

void group::add_module(group::module* ptr)
{
    auto mname = ptr->name();
    std::unique_ptr<group::module> mptr(ptr);
    // lifetime scope of guard
    {
        std::lock_guard<std::mutex> guard(s_mtx);
        if (mmap().insert(std::make_pair(mname, std::move(mptr))).second)
        {
            return; // success; don't throw exception
        }
    }
    std::string error_msg = "module name \"";
    error_msg += mname;
    error_msg += "\" already defined";
    throw std::logic_error(error_msg);

}

group::subscription::subscription(const channel_ptr& s,
                                  const intrusive_ptr<group>& g)
    : m_self(s), m_group(g)
{
}

group::subscription::subscription(group::subscription&& other)
    : m_self(std::move(other.m_self)), m_group(std::move(other.m_group))
{
}

group::subscription::~subscription()
{
    if (m_group) m_group->unsubscribe(m_self);
}

group::group(std::string&& id, std::string&& mod_name)
    : m_identifier(std::move(id)), m_module_name(std::move(mod_name))
{
}

group::group(const std::string& id, const std::string& mod_name)
    : m_identifier(id), m_module_name(mod_name)
{
}

const std::string& group::identifier() const
{
    return m_identifier;
}

const std::string& group::module_name() const
{
    return m_module_name;
}

} // namespace cppa
