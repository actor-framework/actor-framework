#include "cppa/actor_proxy.hpp"
#include "cppa/exit_reason.hpp"
#include "cppa/detail/actor_impl_util.hpp"

using cppa::exit_reason::not_exited;

namespace { typedef std::lock_guard<std::mutex> guard_type; }

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

// implemented in unicast_network.cpp
// void actor_proxy::enqueue(const message& msg);

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
