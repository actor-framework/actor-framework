#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "cppa/actor.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

namespace {

std::atomic<std::uint32_t> s_ids(1);
std::map<std::uint32_t, cppa::actor*> s_instances;
cppa::util::shared_spinlock s_instances_mtx;

typedef std::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;

} // namespace <anonmyous>

namespace cppa {

actor::actor(std::uint32_t aid, const process_information_ptr& pptr)
    : m_is_proxy(true), m_id(aid), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
}

actor::actor(const process_information_ptr& pptr)
    : m_is_proxy(false), m_id(s_ids.fetch_add(1)), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
    else
    {
        exclusive_guard guard(s_instances_mtx);
        s_instances.insert(std::make_pair(m_id, this));
    }
}

actor::~actor()
{
    if (!m_is_proxy)
    {
        exclusive_guard guard(s_instances_mtx);
        s_instances.erase(m_id);
    }
}

intrusive_ptr<actor> actor::by_id(std::uint32_t actor_id)
{
    shared_guard guard(s_instances_mtx);
    auto i = s_instances.find(actor_id);
    if (i != s_instances.end())
    {
        return i->second;
    }
    return nullptr;
}

void actor::join(group_ptr& what)
{
    if (!what) return;
    attach(what->subscribe(this));
}

void actor::leave(const group_ptr& what)
{
    if (!what) return;
    attachable::token group_token(typeid(group::unsubscriber), what.get());
    detach(group_token);
}

void actor::link_to(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    link_to(tmp);
}

void actor::unlink_from(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    unlink_from(tmp);
}

bool actor::remove_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return remove_backlink(tmp);
}

bool actor::establish_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return establish_backlink(tmp);
}

} // namespace cppa
