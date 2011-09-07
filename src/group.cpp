#include "cppa/group.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

#include "cppa/detail/group_manager.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa {

intrusive_ptr<group> group::get(const std::string& arg0,
                                const std::string& arg1)
{
    return detail::singleton_manager::get_group_manager()->get(arg0, arg1);
}

void group::add_module(group::module* ptr)
{
    detail::singleton_manager::get_group_manager()->add_module(ptr);
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
