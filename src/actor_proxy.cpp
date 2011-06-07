#include "cppa/atom.hpp"
#include "cppa/message.hpp"
#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/actor_impl_util.hpp"

using cppa::exit_reason::not_exited;

namespace {

constexpr auto s_kp = cppa::atom(":KillProxy");
typedef std::lock_guard<std::mutex> guard_type;

} // namespace <anonymous>

namespace cppa {

actor_proxy::actor_proxy(std::uint32_t mid, const process_information_ptr& pptr)
    : actor(mid), m_parent(pptr), m_exit_reason(not_exited)
{
    if (!m_parent) throw std::runtime_error("parent == nullptr");
}

actor_proxy::actor_proxy(std::uint32_t mid, process_information_ptr&& pptr)
    : actor(mid), m_parent(std::move(pptr)), m_exit_reason(not_exited)
{
    if (!m_parent) throw std::runtime_error("parent == nullptr");
}

void actor_proxy::enqueue(const message& msg)
{
    const any_tuple& content = msg.content();
    if (   content.size() == 2
        && content.utype_info_at(0) == typeid(atom_value)
        && *reinterpret_cast<const atom_value*>(content.at(0)) == s_kp
        && content.utype_info_at(1) == typeid(std::uint32_t))
    {
        decltype(m_attachables) mattachables;
        auto r = *reinterpret_cast<const std::uint32_t*>(content.at(1));
        // lifetime scope of guard
        {
            guard_type guard(m_mtx);
            m_exit_reason = r;
            mattachables = std::move(m_attachables);
            m_attachables.clear();
        }
        for (auto i = mattachables.begin(); i != mattachables.end(); ++i)
        {
            (*i)->detach(r);
        }
    }
    else
    {
        forward_message(m_parent, msg);
    }
}

bool actor_proxy::attach(attachable* ptr)
{
    return detail::do_attach<guard_type>(m_exit_reason,
                                         unique_attachable_ptr(ptr),
                                         m_attachables,
                                         m_mtx);
}

void actor_proxy::detach(const attachable::token& what)
{
    detail::do_detach<guard_type>(what, m_attachables, m_mtx);
}

void actor_proxy::link_to(intrusive_ptr<actor>& other)
{
}

void actor_proxy::unlink_from(intrusive_ptr<actor>& other)
{
}

bool actor_proxy::remove_backlink(const intrusive_ptr<actor>& to)
{
    return true;
}

bool actor_proxy::establish_backlink(const intrusive_ptr<actor>& to)
{
    return true;
}

const process_information& actor_proxy::parent_process() const
{
    return *m_parent;
}

process_information_ptr actor_proxy::parent_process_ptr() const
{
    return m_parent;
}


} // namespace cppa
