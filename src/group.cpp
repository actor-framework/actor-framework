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

typedef util::shared_spinlock shared_mutex_type;
typedef std::lock_guard<shared_mutex_type> exclusive_guard;
typedef util::shared_lock_guard<shared_mutex_type> shared_guard;
typedef util::upgrade_lock_guard<shared_mutex_type> upgrade_guard;

modules_map s_mmap;
std::mutex s_mmap_mtx;

class local_group : public group
{

    friend class local_group_module;

    shared_mutex_type m_shared_mtx;
    std::set<channel_ptr> m_subscribers;

    // allow access to local_group_module only
    local_group(std::string&& gname) : group(std::move(gname), "local")
    {
    }

    local_group(const std::string& gname) : group(gname, "local")
    {
    }

 public:

    virtual void enqueue(const message& msg)
    {
        shared_guard guard(m_shared_mtx);
        for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
        {
            const_cast<channel_ptr&>(*i)->enqueue(msg);
        }
    }

    virtual group::subscription subscribe(const channel_ptr& who)
    {
        group::subscription result;
        exclusive_guard guard(m_shared_mtx);
        if (m_subscribers.insert(who).second)
        {
            result.reset(new group::unsubscriber(who, this));
        }
        return result;
    }

    virtual void unsubscribe(const channel_ptr& who)
    {
        exclusive_guard guard(m_shared_mtx);
        m_subscribers.erase(who);
    }

};

class local_group_module : public group::module
{

    typedef group::module super;

    util::shared_spinlock m_mtx;
    std::map<std::string, group_ptr> m_instances;


 public:

    local_group_module() : super("local")
    {
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
            group_ptr tmp(new local_group(group_name));
            // lifetime scope of uguard
            {
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(group_name, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

};

// @pre: s_mmap_mtx is locked
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
        std::lock_guard<std::mutex> guard(s_mmap_mtx);
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
        std::lock_guard<std::mutex> guard(s_mmap_mtx);
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

group::unsubscriber::unsubscriber(const channel_ptr& s,
                                  const intrusive_ptr<group>& g)
    : m_self(s), m_group(g)
{
}

group::unsubscriber::~unsubscriber()
{
    if (m_group) m_group->unsubscribe(m_self);
}

group::module::module(const std::string& name) : m_name(name)
{
}

group::module::module(std::string&& name) : m_name(std::move(name))
{
}

const std::string& group::module::name()
{
    return m_name;
}

bool group::unsubscriber::matches(const attachable::token& what)
{
    if (what.subtype == typeid(group::unsubscriber))
    {
        return m_group == reinterpret_cast<const group*>(what.ptr);
    }
    return false;
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
