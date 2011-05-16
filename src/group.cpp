#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <memory>
#include <stdexcept>

#include "cppa/group.hpp"

namespace cppa {

namespace {

typedef std::map< std::string, std::unique_ptr<group::module> > modules_map;

std::mutex s_mtx;
modules_map s_mmap;

} // namespace <anonymous>

intrusive_ptr<group> group::get(const std::string& module_name,
                                const std::string& group_name)
{
    std::lock_guard<std::mutex> guard(s_mtx);
    auto i = s_mmap.find(module_name);
    if (i != s_mmap.end())
    {
        return (i->second)->get(group_name);
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
        if (!s_mmap.insert(std::make_pair(mname, std::move(mptr))).second)
        {
            std::string error_msg = "module name \"";
            error_msg += mname;
            error_msg += "\" already defined";
            throw std::logic_error(error_msg);
        }
    }
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

} // namespace cppa
